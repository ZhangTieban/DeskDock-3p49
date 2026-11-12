    const petData = __PET_DATA__;
    const clockFonts = __CLOCK_FONT_DATA__;
    const clockPixels = root.querySelector('.dock-clock-bitmap');
    const periodPixels = root.querySelector('.dock-period-bitmap');
    const datePixels = root.querySelector('.dock-date-bitmap');
    let lastClockSignature='';
    function inkBottom(font,text){return Math.max(0,...[...text].map(c=>font.height-font.base-font.glyphs[c].y));}
    function clockBitmap(canvas,font,text,right){
      const context=canvas.getContext('2d');
      context.clearRect(0,0,canvas.width,canvas.height);
      if(!text)return;
      const width=[...text].reduce((sum,c)=>sum+font.glyphs[c].advance,0);
      let pen=right?canvas.width-width:Math.floor((canvas.width-width)/2);
      const image=context.createImageData(canvas.width,canvas.height);
      for(const c of text){
        const g=font.glyphs[c],stride=Math.ceil(g.w/2);
        const top=font.height-font.base-g.h-g.y;
        for(let y=0;y<g.h;y++)for(let x=0;x<g.w;x++){
          const byte=parseInt(g.bits.slice((y*stride+(x>>1))*2,(y*stride+(x>>1))*2+2),16);
          const alpha=((x&1)?byte&15:byte>>4)*17;
          const dx=pen+g.x+x,dy=top+y;
          if(dx>=0&&dx<canvas.width&&dy>=0&&dy<canvas.height&&alpha)
            image.data.set([243,245,242,alpha],(dy*canvas.width+dx)*4);
        }
        pen+=g.advance;
      }
      context.putImageData(image,0,0);
    }
    function paintClockBitmap(text,period){
      const signature=text+'|'+period;
      if(signature===lastClockSignature)return;
      lastClockSignature=signature;
      clockBitmap(clockPixels,clockFonts['88'],text,false);
      clockBitmap(periodPixels,clockFonts['14_bold'],period,true);
      if(period){
        const y=3+inkBottom(clockFonts['88'],text)-inkBottom(clockFonts['14_bold'],period);
        periodPixels.style.top=periodLabel.style.top=(y/6.4)+'cqw';
      }
    }
    function paintDateBitmap(text){clockBitmap(datePixels,clockFonts['16_bold'],text,false);}
    const petContext = dog.getContext('2d');
    const zoom = root.querySelector('.pet-zoom');
    const zoomContext = zoom.getContext('2d');
    const audioCanvas = root.querySelector('.dock-audio');
    const audioChoice = root.querySelector('[data-control=audio]');
    const hourChoice = root.querySelector('[data-control=hour12]');
    const periodLabel = root.querySelector('.dock-period');
    const activityLabel = root.querySelector('.pet-current');
    const pauseButton = root.querySelector('[data-action=pause]');
    let tick=0, actionTick=0, dayIndex=0, showcase=0, previous='', heartUntil=0;
    let paused=window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    const dayActions=['walk','run','ball','sit','fetch','pet','frisbee','tug'];
    const colors=petData.palette.map(c=>'#'+c);
    function scheduleDog() {
      if (motion.value==='showcase') return petData.actions[showcase];
      if (motion.value!=='auto') return motion.value;
      const [hour,minute]=timeInput.value.split(':').map(Number);
      const m=hour*60+minute;
      if (m>=1320 || m<420 || (m>=780 && m<840)) return 'sleep';
      if ((m>=480 && m<490) || (m>=1080 && m<1110)) return 'eat';
      if (m>=1050 && m<1080) return 'leash';
      return dayActions[dayIndex];
    }
    function petSprite(index,x,y,flip=false) {
      const rows=petData.sprites[index];
      for (let yy=0;yy<rows.length;yy++) for(let xx=0;xx<rows[0].length;xx++) {
        const c=rows[yy][flip?rows[0].length-1-xx:xx];
        if(c==='.') continue;
        petContext.fillStyle=colors[parseInt(c,16)];
        petContext.fillRect(x+xx,y+yy,1,1);
      }
    }
    function paintAudio() {
      const c=audioCanvas.getContext('2d'); c.clearRect(0,0,12,12);
      const icons=[
        ['............','.........1..','......111...','...111......','..11111111..','..1......1..','..1..11..1..','..1..11..1..','..1......1..','..11111111..','............','............'],
        ['............','....111111..','....1....1..','....111111..','....1....1..','....1....1..','....1....1..','..111..111..','.1111.1111..','.111..111...','............','............']
      ];
      const type=audioChoice.value;
      if(type!=='radio' && type!=='mp3') return;
      c.fillStyle='#81cbb6';
      icons[type==='mp3'?1:0].forEach((row,y)=>[...row].forEach((p,x)=>{if(p==='1')c.fillRect(x,y,1,1);}));
    }
    function paintPet() {
      const name=scheduleDog();
      if(name!==previous){actionTick=motion.value==='leash'?120:0;previous=name;}
      const index=petData.actions.indexOf(name), frames=petData.frames[index];
      const f=frames[actionTick%frames.length];
      petContext.fillStyle=colors[0]; petContext.fillRect(0,0,296,32);
      if(f[10]) {
        const x1=f[4]+(f[6]?3:15),y1=16,x2=f[0]+(f[3]?8:24),y2=21;
        const length=Math.max(Math.abs(x2-x1),Math.abs(y2-y1));
        petContext.fillStyle='#81cbb6';
        for(let i=0;i<=length;i++)petContext.fillRect(x1+Math.trunc((x2-x1)*i/Math.max(1,length)),y1+Math.trunc((y2-y1)*i/Math.max(1,length)),1,1);
      }
      petSprite(f[5]+9,f[4],0,Boolean(f[6]));
      petSprite(f[2],f[0],f[1],Boolean(f[3]));
      petSprite(f[9],f[7],f[8]);
      if(tick<heartUntil)petSprite(20,f[0]+12,4+Math.floor(tick/5)%2);
      zoomContext.imageSmoothingEnabled=false;
      zoomContext.drawImage(dog,0,0,592,64);
      const caption=petData.labels[index]+(motion.value==='showcase'?' · '+(index+1)+' / '+petData.actions.length:'');
      if(activityLabel.textContent!==caption)activityLabel.textContent=caption;
      dog.dataset.action=name; dog.dataset.frame=String(actionTick%frames.length);
      const [h,m]=timeInput.value.split(':');
      const valid=/^\d{2}:\d{2}$/.test(timeInput.value);
      clock.textContent=valid?(hourChoice.checked?String(Number(h)%12||12).padStart(2,'0')+':'+m:timeInput.value):'--:--';
      periodLabel.textContent=valid&&hourChoice.checked?(Number(h)<12?'AM':'PM'):'';
      paintClockBitmap(clock.textContent,periodLabel.textContent);
      paintAudio();
    }
    function advancePet() {
      const name=scheduleDog(), index=petData.actions.indexOf(name);
      tick++;actionTick++;
      if(actionTick>=petData.frames[index].length){
        actionTick=0;
        if(motion.value==='showcase')showcase=(showcase+1)%petData.actions.length;
        else if(motion.value==='auto' && !['sleep','eat','leash'].includes(name))dayIndex=(dayIndex+1)%dayActions.length;
      }
      paintPet();
    }
    function savePetState(){
      window.openai?.setWidgetState?.({modelContent:{action:motion.value,time:timeInput.value,audio:audioChoice.value,hour12:hourChoice.checked},privateContent:{date:input.value}})?.catch(()=>{});
    }
    function restorePetState(state){
      if(!state)return;
      const s=state.modelContent||{};
      if([...motion.options].some(o=>o.value===s.action))motion.value=s.action;
      if(/^\d{2}:\d{2}$/.test(s.time||''))timeInput.value=s.time;
      if([...audioChoice.options].some(o=>o.value===s.audio))audioChoice.value=s.audio;
      hourChoice.checked=Boolean(s.hour12);
      if(state.privateContent?.date)input.value=state.privateContent.date;
      actionTick=0;previous='';paintPet();
    }
    function changeDogMode(){actionTick=0;previous='';paintPet();savePetState();}
    motion.addEventListener('change',changeDogMode);
    timeInput.addEventListener('input',changeDogMode);
    audioChoice.addEventListener('change',()=>{paintAudio();savePetState();});
    hourChoice.addEventListener('change',()=>{paintPet();savePetState();});
    root.querySelector('[data-action=heart]').addEventListener('click',()=>{heartUntil=tick+30;paintPet();});
    root.querySelector('.dock-lcd').addEventListener('pointerdown',()=>{heartUntil=tick+30;paintPet();});
    pauseButton.textContent=paused?'繼續播放':'暫停動作';
    pauseButton.addEventListener('click',()=>{paused=!paused;pauseButton.textContent=paused?'繼續播放':'暫停動作';});
    root.querySelector('[data-action=step]').addEventListener('click',()=>{paused=true;pauseButton.textContent='繼續播放';advancePet();});
    window.setInterval(()=>{if(!paused)advancePet();},80);
    restorePetState(window.openai?.widgetState);
    window.addEventListener('openai:set_globals',event=>{restorePetState(event.detail?.globals?.widgetState);render();});
    paintPet();
