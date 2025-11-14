const G_UPS=128;
const UPDATE_INTERVAL = 1000 / G_UPS; // 7.8125 мс
var g_qDev={};
var qDev={
  parr:[-1,-1,+1,-1,+1,+1,-1,+1].map(e=>e*256),
  // Цвета теперь uint8_t RGBA (Direct3D9), например: [255,255,255,255, 255,0,0,255, ...]
  // (см. fill_carr и set_carr ниже)
  carr: new Uint8Array([
    255,255,255,255, // white
    255,0,0,255,     // red
    0,255,0,255,     // green
    0,0,255,255      // blue
  ]),
  tarr:[0,0,1,0,1,1,0,1],
  iarr:[0,1,2,0,2,3],
  parr_buff:null,
  carr_buff:null,
  iarr_buff:null,
  gl:null,
  prog:null,
  vbo:null,
  ibo:null,
  NextFrame:(qDev)=>{
    qDev.buffs_pos=0;
    if(!qDev.buffs.length)return;
    let e=qDev.buffs[qDev.buffs_pos];
    qDev.vbo=e.vbo;
    qDev.ibo=e.ibo;
  },
  DIP_v2:(qDev,pvb,pib,vc,ic)=>{
    let gl=qDev.gl;
    qDev.make_all(qDev,pvb,pib,vc,ic);
    qDev.link_all(qDev);
    gl.drawElements(gl.TRIANGLES,ic,gl.UNSIGNED_INT,0);
    qDev.buffs_pos++;
    if(qDev.buffs_pos>=qDev.buffs.length){
      qDev.buffs.push({vbo:qDev.vbo,ibo:qDev.ibo});
      qDev.vbo=null;
      qDev.ibo=null;
    }else{
      let e=qDev.buffs[qDev.buffs_pos];
      qDev.vbo=e.vbo;
      qDev.ibo=e.ibo;
    }
  },
  make_all:(qDev,pvb,pib,vc,ic)=>{
    let gl=qDev.gl;
    let vbv=new Uint8Array(HEAPU8.buffer,pvb,vc*qDev.stride);
    let ibv=new Uint32Array(HEAPU32.buffer,pib,ic);
    if(!qDev.vbo)qDev.vbo=gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,qDev.vbo);
    gl.bufferData(gl.ARRAY_BUFFER,vbv,gl.DYNAMIC_DRAW);
    if(!qDev.ibo)qDev.ibo=gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,qDev.ibo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,ibv,gl.DYNAMIC_DRAW);
  },
  stride:3*4+1*4+2*4, // 3*float + 1*uint32 + 2*float = 24 bytes
  link_all:(qDev)=>{
    let gl=qDev.gl; let a='attribLocations';
    // Позиция (vec3): offset 0, 3 float
    gl.vertexAttribPointer(qDev.prog[a].vpos,3,gl.FLOAT,false,qDev.stride,0);
    gl.enableVertexAttribArray(qDev.prog[a].vpos);
    // Цвет (uint32, RGBA): offset 12, 4 ubyte, normalized
    gl.vertexAttribPointer(qDev.prog[a].vcol,4,gl.UNSIGNED_BYTE,true,qDev.stride,12);
    gl.enableVertexAttribArray(qDev.prog[a].vcol);
    // UV (vec2): offset 16, 2 float
    gl.vertexAttribPointer(qDev.prog[a].vtex,2,gl.FLOAT,false,qDev.stride,16);
    gl.enableVertexAttribArray(qDev.prog[a].vtex);
  },
  fill_parr:(qDev)=>{
    let gl=qDev.gl;
    if(!qDev.parr_buff)qDev.parr_buff=gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,qDev.parr_buff);
    gl.bufferData(gl.ARRAY_BUFFER,qDev.parr,gl.DYNAMIC_DRAW);
  },
  fill_carr:(qDev)=>{
    let gl=qDev.gl;
    if(!qDev.carr_buff)qDev.carr_buff=gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,qDev.carr_buff);
    gl.bufferData(gl.ARRAY_BUFFER,qDev.carr,gl.DYNAMIC_DRAW);
  },
  fill_iarr:(qDev)=>{
    let gl=qDev.gl;
    if(!qDev.iarr_buff)qDev.iarr_buff=gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,qDev.iarr_buff);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,qDev.iarr,gl.DYNAMIC_DRAW);
  },
  fill_tarr:(qDev)=>{
    let gl=qDev.gl;
    if(!qDev.tarr_buff)qDev.tarr_buff=gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,qDev.tarr_buff);
    gl.bufferData(gl.ARRAY_BUFFER,qDev.tarr,gl.DYNAMIC_DRAW);
  },
  set_parr:(qDev,prog)=>{
    let gl=qDev.gl;
    gl.bindBuffer(gl.ARRAY_BUFFER,qDev.parr_buff);
    gl.vertexAttribPointer(
      prog.attribLocations.vpos,
      2, gl.FLOAT, false, 0, 0
    );
    gl.enableVertexAttribArray(prog.attribLocations.vpos);
  },
  set_carr:(qDev,prog)=>{
    let gl=qDev.gl;
    gl.bindBuffer(gl.ARRAY_BUFFER,qDev.carr_buff);
    // Важно: используем UNSIGNED_BYTE, normalized=true!
    gl.vertexAttribPointer(
      prog.attribLocations.vcol,
      4, gl.UNSIGNED_BYTE, true, 0, 0
    );
    gl.enableVertexAttribArray(prog.attribLocations.vcol);
  },
  set_iarr:qDev=>{
    let gl=qDev.gl;
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,qDev.iarr_buff);
  },
  set_tarr:(qDev,prog)=>{
    let gl=qDev.gl;
    gl.bindBuffer(gl.ARRAY_BUFFER,qDev.tarr_buff);
    gl.vertexAttribPointer(
      prog.attribLocations.vtex,
      2, gl.FLOAT, false, 0, 0
    );
    gl.enableVertexAttribArray(prog.attribLocations.vtex);
  },
  fill_buffs:qDev=>{
    qDev.fill_parr(qDev);
    qDev.fill_carr(qDev);
    qDev.fill_iarr(qDev);
    qDev.fill_tarr(qDev);
  },
  set_buffs:(qDev)=>{
    qDev.set_parr(qDev,qDev.prog);
    qDev.set_carr(qDev,qDev.prog);
    qDev.set_iarr(qDev,qDev.prog);
    qDev.set_tarr(qDev,qDev.prog);
  },
  DIP:(qDev)=>{
    let gl=qDev.gl;
    qDev.fill_buffs(qDev);
    qDev.set_buffs(qDev);
    gl.drawElements(gl.TRIANGLES,qDev.iarr.length,gl.UNSIGNED_INT,0);
    qDev.buffs_pos++;
    if(qDev.buffs_pos>=qDev.buffs.length){
      qDev.buffs.push({
        parr_buff:qDev.parr_buff,
        carr_buff:qDev.carr_buff,
        tarr_buff:qDev.tarr_buff,
        iarr_buff:qDev.iarr_buff,
      });
      qDev.parr_buff=null;
      qDev.carr_buff=null;
      qDev.tarr_buff=null;
      qDev.iarr_buff=null;
    }else{
      let e=qDev.buffs[qDev.buffs_pos];
      qDev.parr_buff=e.parr_buff;
      qDev.carr_buff=e.carr_buff;
      qDev.tarr_buff=e.tarr_buff;
      qDev.iarr_buff=e.iarr_buff;
    }
  },
  buffs:[],
  buffs_pos:0,
  NextFrame_old:(qDev)=>{
    qDev.buffs_pos=0;
    if(qDev.buffs.length==0)return;
    let e=qDev.buffs[qDev.buffs_pos];
    qDev.parr_buff=e.parr_buff;
    qDev.carr_buff=e.carr_buff;
    qDev.tarr_buff=e.tarr_buff;
    qDev.iarr_buff=e.iarr_buff;
  }
};
function initFont(fontSize,fontFamily,texSize,bold){
  var out={W:[],H:[],wh:texSize};
  var canvas=document.createElement('canvas');
  canvas.style="display:none";
  var ctx=canvas.getContext('2d');
  document.body.appendChild(canvas);
  canvas.width=texSize;canvas.height=texSize;
  //ctx.textAlign    = 'left';
  //ctx.textBaseline = 'base';
  ctx.fillStyle = 'black';
  ctx.fillRect(0,0,canvas.width,canvas.height);
  ctx.fillStyle    = 'white';
  //ctx.textAlign    = 'left';
  //ctx.textBaseline = 'base';
  //ctx.font         = "20px Consolas";//fontSize+'px '+fontFamily;
  //out.font=fontSize+'px '+fontFamily;
  bold=true;//fontFamily="Consolas";
  ctx.font         = (bold?"bold ":"")+fontSize+'pt '+fontFamily;//out.font;
  let m=ctx.measureText('_');let gH=m.fontBoundingBoxAscent+m.fontBoundingBoxDescent;
  for(let j=0;j<=255;j++){
    let c=String.fromCharCode(j);let i=j;
    let m=ctx.measureText(c);
    let x=(i%16)*(texSize/16);let y=((i/16)|0)*(texSize/16)-gH/4;
    let H=gH;//m.fontBoundingBoxAscent+m.fontBoundingBoxDescent;
    ctx.fillText(c,x,y+H);
    out.W[i]=m.width;
    out.H[i]=H;
  }
  out.data=ctx.getImageData(0,0,texSize,texSize).data;
  out.ctx=ctx;
  canvas.remove();
  return out;
}
function initFont_v2(font,dest,pW,pH){
  let p=font.data;
  let n=font.wh*font.wh*4;
  for(let i=0;i<n;i+=4){
    HEAPU8[dest+i+0]=255;
    HEAPU8[dest+i+1]=255;
    HEAPU8[dest+i+2]=255;
    HEAPU8[dest+i+3]=p[i+0];//!=255?0:255;
  }
  n=font.W.length;
  p=font.W;
  d=pW>>2;
  for(let i=0;i<n;i++){
    HEAPU32[d+i]=p[i];
  }
  n=font.H.length;
  p=font.H;
  d=pH>>2;
  for(let i=0;i<n;i++){
    HEAPU32[d+i]=p[i];
  }
}
function drawScene(gl,prog,buffers,squareRotation) {
  let c=210/255;
  gl.clearColor(c,c,c,1.0);
  gl.clear(gl.COLOR_BUFFER_BIT);
  gl.disable(gl.DEPTH_TEST);
  //gl.clearDepth(1.0);
  //gl.enable(gl.DEPTH_TEST);
  //gl.depthFunc(gl.LEQUAL);// Near things obscure far things

  // Clear the canvas before we start drawing on it.

  //gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

  gl.enable(gl.BLEND);
  gl.blendFunc(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA);

  var OrthoLH=(w,h,zn,zf)=>{
    let mat=mat4.fromValues(
      2.0/w,0,0,0,
      0,2.0/h,0,0,
      0,0,1/(zf-zn),0,
      0,0,-zn/(zf-zn),1
    );
    return mat;
  }
  let w=gl.canvas.clientWidth
  let h=gl.canvas.clientHeight;
  let projectionMatrix=OrthoLH(w,h,0.1,100);

  // Set the drawing position to the "identity" point, which is
  // the center of the scene.
  const modelViewMatrix = mat4.create();

  // Now move the drawing position a bit to where we want to
  // start drawing the square.
  mat4.translate(
    modelViewMatrix, // destination matrix
    modelViewMatrix, // matrix to translate
    [-0.0, 0.0, +0.5]
  ); // amount to translate


  // Tell WebGL to use our program when drawing
  gl.useProgram(prog.program);

  // Set the shader uniforms
  gl.uniformMatrix4fv(
    prog.uniformLocations.projectionMatrix,
    false,
    projectionMatrix
  );
  gl.uniformMatrix4fv(
    prog.uniformLocations.modelViewMatrix,
    false,
    modelViewMatrix
  );
  gl.uniform1f(prog.uniformLocations.use_tex,1.0);
  gl.activeTexture(gl.TEXTURE0);
  gl.bindTexture(gl.TEXTURE_2D,null);
  gl.uniform1i(prog.uniformLocations.uSampler,0);

  qDev.prog=prog;
  //qDev.parr[0]=squareRotation;
  qDev.NextFrame(qDev);
  Module.ccall('render','int',["int"],[0]);
  //qDev.DIP(qDev);
}

var squareRotation=-256;
let g_render=[];

function main() {
  const canvas = document.querySelector("#glcanvas");
  const gl = canvas.getContext("webgl",{ alpha: false,premultipliedAlpha: false,antialias: true});
  const ext = gl.getExtension("OES_element_index_uint");
  if (gl===null||ext===null) {
    alert(
      "Unable to initialize WebGL. Your browser or machine may not support it."
    );
    return;
  }

  let c=210/255;
  gl.clearColor(c,c,c,1.0);
  gl.clear(gl.COLOR_BUFFER_BIT);
  
  const vsSource = `
    attribute vec4 vpos;
    attribute vec4 vcol; // RGBA из JS (Direct3D9)
    attribute vec2 vtex;

    uniform mat4 uModelViewMatrix;
    uniform mat4 uProjectionMatrix;

    varying highp vec4 vColor;
    varying highp vec2 vTextureCoord;

    void main(void) {
      gl_Position = uProjectionMatrix * uModelViewMatrix * vpos;
      // Меняем R и B местами: vcol.bgra = vcol.rgba
      vColor = vcol.bgra;
      vTextureCoord = vtex;
    }
  `;


  const fsSource = `
    precision highp float;
    varying highp vec4 vColor;
    varying highp vec2 vTextureCoord;
    uniform sampler2D uSampler;
    uniform float use_tex;

    void main(void) {
      vec4 t=use_tex==1.0?texture2D(uSampler,vTextureCoord):vec4(1,1,1,1);
      gl_FragColor = t*vColor;
      gl_FragColor.rgb *= gl_FragColor.a;
    }
  `;

  const shaderProgram = initShaderProgram(gl, vsSource, fsSource);

  const prog={
    program:shaderProgram,
    attribLocations:{
      vpos:gl.getAttribLocation(shaderProgram,"vpos"),
      vcol:gl.getAttribLocation(shaderProgram,"vcol"),
      vtex:gl.getAttribLocation(shaderProgram,"vtex"),
    },
    uniformLocations:{
      projectionMatrix:gl.getUniformLocation(shaderProgram,"uProjectionMatrix"),
      modelViewMatrix: gl.getUniformLocation(shaderProgram,"uModelViewMatrix"),
      uSampler:        gl.getUniformLocation(shaderProgram,"uSampler"),
      use_tex:         gl.getUniformLocation(shaderProgram,"use_tex"),
    },
  };

  qDev.gl=gl;

  //const texture=loadTexture(gl,"cubetexture.png");
  //gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL,true);

  let then = 0;

  function render(now) {
    now *= 0.001;
    deltaTime = now - then;
    then = now;
    drawScene(gl,prog,null,squareRotation);
    g_dontupdate=false;
    for(;!g_dontupdate;){
      if(performance.now()-g_lastUpdate>=UPDATE_INTERVAL){
        if(!g_dontupdate)Module.ccall('update','int',["int"],[0]);
        g_lastUpdate+=UPDATE_INTERVAL;
      }else break;
    }
    //squareRotation += deltaTime;
    requestAnimationFrame(render);
  }
  requestAnimationFrame(render);
  g_render=render;
}
function start_render(){
  g_lastUpdate=performance.now();
  requestAnimationFrame(g_render);
}
let g_lastUpdate=performance.now();let g_dontupdate=false;

function update_kb(pD,pC){
  let MAX_KEY=263;
  for(let i=0;i<=MAX_KEY;i++){
    HEAPU8[pD+i]=g_kb_down[i];
    HEAPU8[pC+i]=g_kb_changed[i];
    g_kb_changed[i]=false;
    g_zDelta=0;
  }
}
//https://gist.github.com/adler3d/7f18282ea9ef553d5fa03a101bf4c3d0
var g_keys={"0":{"v":["VK_NUMPAD0"],"k":[96]},
"1":{"v":["VK_NUMPAD1"],"k":[97]},
"2":{"v":["VK_NUMPAD2"],"k":[98]},
"3":{"v":["VK_NUMPAD3"],"k":[99]},
"4":{"v":["VK_NUMPAD4"],"k":[100]},
"5":{"v":["VK_NUMPAD5"],"k":[101]},
"6":{"v":["VK_NUMPAD6"],"k":[102]},
"7":{"v":["VK_NUMPAD7"],"k":[103]},
"8":{"v":["VK_NUMPAD8"],"k":[104]},
"9":{"v":["VK_NUMPAD9"],"k":[105]},
"Alt":{"v":["VK_MENU","VK_LMENU","VK_RMENU"],"k":[18,164,165]},
"CapsLock":{"v":["VK_CAPITAL"],"k":[20]},
"Control":{"v":["VK_CONTROL","VK_LCONTROL","VK_RCONTROL"],"k":[17,162,163]},
"Meta":{"v":["VK_LWIN","VK_RWIN"],"k":[91,92]},
"NumLock":{"v":["VK_NUMLOCK"],"k":[144]},
"ScrollLock":{"v":["VK_SCROLL"],"k":[145]},
"Shift":{"v":["VK_SHIFT","VK_LSHIFT","VK_RSHIFT"],"k":[16,160,161]},
"Enter":{"v":["VK_RETURN"],"k":[13]},
"Tab":{"v":["VK_TAB"],"k":[9]},
" ":{"v":["VK_SPACE"],"k":[32]},
"ArrowDown":{"v":["VK_DOWN"],"k":[40]},
"ArrowLeft":{"v":["VK_LEFT"],"k":[37]},
"ArrowRight":{"v":["VK_RIGHT"],"k":[39]},
"ArrowUp":{"v":["VK_UP"],"k":[38]},
"End":{"v":["VK_END"],"k":[35]},
"Home":{"v":["VK_HOME"],"k":[36]},
"PageDown":{"v":["VK_NEXT"],"k":[34]},
"PageUp":{"v":["VK_PRIOR"],"k":[33]},
"Backspace":{"v":["VK_BACK"],"k":[8]},
"Clear":{"v":["VK_CLEAR","VK_OEM_CLEAR"],"k":[12,254]},
"Copy":{"v":["APPCOMMAND_COPY"],"k":[36]},
"CrSel":{"v":["VK_CRSEL"],"k":[247]},
"Cut":{"v":["APPCOMMAND_CUT"],"k":[37]},
"Delete":{"v":["VK_DELETE"],"k":[46]},
"EraseEof":{"v":["VK_EREOF"],"k":[249]},
"ExSel":{"v":["VK_EXSEL"],"k":[248]},
"Insert":{"v":["VK_INSERT"],"k":[45]},
"Paste":{"v":["APPCOMMAND_PASTE"],"k":[38]},
"Redo":{"v":["APPCOMMAND_REDO"],"k":[35]},
"Undo":{"v":["APPCOMMAND_UNDO"],"k":[34]},
"Accept":{"v":["VK_ACCEPT"],"k":[30]},
"Attn":{"v":["VK_OEM_ATTN"],"k":[240]},
"ContextMenu":{"v":["VK_APPS"],"k":[93]},
"Escape":{"v":["VK_ESCAPE"],"k":[27]},
"Execute":{"v":["VK_EXECUTE"],"k":[43]},
"Find":{"v":["APPCOMMAND_FIND"],"k":[28]},
"Finish":{"v":["VK_OEM_FINISH"],"k":[241]},
"Help":{"v":["VK_HELP","APPCOMMAND_HELP"],"k":[47,27]},
"Pause":{"v":["VK_PAUSE"],"k":[19]},
"Play":{"v":["VK_PLAY"],"k":[250]},
"Select":{"v":["VK_SELECT"],"k":[41]},
"PrintScreen":{"v":["VK_SNAPSHOT"],"k":[44]},
"Standby":{"v":["VK_SLEEP"],"k":[95]},
"Alphanumeric":{"v":["VK_OEM_ATTN"],"k":[240]},
"Convert":{"v":["VK_CONVERT"],"k":[28]},
"FinalMode":{"v":["VK_FINAL"],"k":[24]},
"ModeChange":{"v":["VK_MODECHANGE"],"k":[31]},
"NonConvert":{"v":["VK_NONCONVERT"],"k":[29]},
"Process":{"v":["VK_PROCESSKEY"],"k":[229]},
"HangulMode":{"v":["VK_HANGUL"],"k":[21]},
"HanjaMode":{"v":["VK_HANJA"],"k":[25]},
"JunjaMode":{"v":["VK_JUNJA"],"k":[23]},
"Hankaku":{"v":["VK_OEM_AUTO"],"k":[243]},
"Hiragana":{"v":["VK_OEM_COPY"],"k":[242]},
"KanaMode":{"v":["VK_KANA","VK_ATTN"],"k":[21,246]},
"KanjiMode":{"v":["VK_KANJI"],"k":[25]},
"Katakana":{"v":["VK_OEM_FINISH"],"k":[241]},
"Romaji":{"v":["VK_OEM_BACKTAB"],"k":[245]},
"Zenkaku":{"v":["VK_OEM_ENLW"],"k":[244]},
"F1":{"v":["VK_F1"],"k":[112]},
"F2":{"v":["VK_F2"],"k":[113]},
"F3":{"v":["VK_F3"],"k":[114]},
"F4":{"v":["VK_F4"],"k":[115]},
"F5":{"v":["VK_F5"],"k":[116]},
"F6":{"v":["VK_F6"],"k":[117]},
"F7":{"v":["VK_F7"],"k":[118]},
"F8":{"v":["VK_F8"],"k":[119]},
"F9":{"v":["VK_F9"],"k":[120]},
"F10":{"v":["VK_F10"],"k":[121]},
"F11":{"v":["VK_F11"],"k":[122]},
"F12":{"v":["VK_F12"],"k":[123]},
"F13":{"v":["VK_F13"],"k":[124]},
"F14":{"v":["VK_F14"],"k":[125]},
"F15":{"v":["VK_F15"],"k":[126]},
"F16":{"v":["VK_F16"],"k":[127]},
"F17":{"v":["VK_F17"],"k":[128]},
"F18":{"v":["VK_F18"],"k":[129]},
"F19":{"v":["VK_F19"],"k":[130]},
"F20":{"v":["VK_F20"],"k":[131]},
"ChannelDown":{"v":["APPCOMMAND_MEDIA_CHANNEL_DOWN"],"k":[52]},
"ChannelUp":{"v":["APPCOMMAND_MEDIA_CHANNEL_UP"],"k":[51]},
"MediaFastForward":{"v":["APPCOMMAND_MEDIA_FAST_FORWARD"],"k":[49]},
"MediaPause":{"v":["APPCOMMAND_MEDIA_PAUSE"],"k":[47]},
"MediaPlay":{"v":["APPCOMMAND_MEDIA_PLAY"],"k":[46]},
"MediaPlayPause":{"v":["VK_MEDIA_PLAY_PAUSE","APPCOMMAND_MEDIA_PLAY_PAUSE"],"k":[179,14]},
"MediaRecord":{"v":["APPCOMMAND_MEDIA_RECORD"],"k":[48]},
"MediaRewind":{"v":["APPCOMMAND_MEDIA_REWIND"],"k":[50]},
"MediaStop":{"v":["VK_MEDIA_STOP","APPCOMMAND_MEDIA_STOP"],"k":[178,13]},
"MediaTrackNext":{"v":["VK_MEDIA_NEXT_TRACK","APPCOMMAND_MEDIA_NEXTTRACK"],"k":[176,11]},
"MediaTrackPrevious":{"v":["VK_MEDIA_PREV_TRACK","APPCOMMAND_MEDIA_PREVIOUSTRACK"],"k":[177,12]},
"AudioBassDown":{"v":["APPCOMMAND_BASS_DOWN"],"k":[19]},
"AudioBassBoostToggle":{"v":["APPCOMMAND_BASS_BOOST"],"k":[20]},
"AudioBassUp":{"v":["APPCOMMAND_BASS_UP"],"k":[21]},
"AudioTrebleDown":{"v":["APPCOMMAND_TREBLE_DOWN"],"k":[22]},
"AudioTrebleUp":{"v":["APPCOMMAND_TREBLE_UP"],"k":[23]},
"AudioVolumeDown":{"v":["VK_VOLUME_DOWN","APPCOMMAND_VOLUME_DOWN"],"k":[174,9]},
"AudioVolumeMute":{"v":["VK_VOLUME_MUTE","APPCOMMAND_VOLUME_MUTE"],"k":[173,8]},
"AudioVolumeUp":{"v":["VK_VOLUME_UP","APPCOMMAND_VOLUME_UP"],"k":[175,10]},
"MicrophoneToggle":{"v":["APPCOMMAND_MIC_ON_OFF_TOGGLE"],"k":[44]},
"MicrophoneVolumeDown":{"v":["APPCOMMAND_MICROPHONE_VOLUME_DOWN"],"k":[25]},
"MicrophoneVolumeMute":{"v":["APPCOMMAND_MICROPHONE_VOLUME_MUTE"],"k":[24]},
"MicrophoneVolumeUp":{"v":["APPCOMMAND_MICROPHONE_VOLUME_UP"],"k":[26]},
"MediaApps":{"v":["VK_APPS"],"k":[93]},
"ZoomToggle":{"v":["VK_ZOOM"],"k":[251]},
"SpeechCorrectionList":{"v":["APPCOMMAND_CORRECTION_LIST"],"k":[45]},
"SpeechInputToggle":{"v":["APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE"],"k":[43]},
"LaunchCalculator":{"v":["APPCOMMAND_LAUNCH_APP2"],"k":[18]},
"LaunchMail":{"v":["VK_LAUNCH_MAIL","APPCOMMAND_LAUNCH_MAIL"],"k":[180,15]},
"LaunchMediaPlayer":{"v":["VK_LAUNCH_MEDIA_SELECT","APPCOMMAND_LAUNCH_MEDIA_SELECT"],"k":[181,16]},
"LaunchMyComputer":{"v":["APPCOMMAND_LAUNCH_APP1"],"k":[17]},
"LaunchApplication1":{"v":["VK_LAUNCH_APP1","APPCOMMAND_LAUNCH_APP1"],"k":[182,17]},
"LaunchApplication2":{"v":["VK_LAUNCH_APP2","APPCOMMAND_LAUNCH_APP2"],"k":[183,18]},
"BrowserBack":{"v":["VK_BROWSER_BACK","APPCOMMAND_BROWSER_BACKWARD"],"k":[166,1]},
"BrowserFavorites":{"v":["VK_BROWSER_FAVORITES","APPCOMMAND_BROWSER_FAVORITES"],"k":[171,6]},
"BrowserForward":{"v":["VK_BROWSER_FORWARD","APPCOMMAND_BROWSER_FORWARD"],"k":[167,2]},
"BrowserHome":{"v":["VK_BROWSER_HOME","APPCOMMAND_BROWSER_HOME"],"k":[172,7]},
"BrowserRefresh":{"v":["VK_BROWSER_REFRESH","APPCOMMAND_BROWSER_REFRESH"],"k":[168,3]},
"BrowserSearch":{"v":["VK_BROWSER_SEARCH","APPCOMMAND_BROWSER_SEARCH"],"k":[170,5]},
"BrowserStop":{"v":["VK_BROWSER_STOP","APPCOMMAND_BROWSER_STOP"],"k":[169,4]},
"Decimal":{"v":["VK_DECIMAL"],"k":[110]},
"Multiply":{"v":["VK_MULTIPLY"],"k":[106]},
"Add":{"v":["VK_ADD"],"k":[107]},
"Divide":{"v":["VK_DIVIDE"],"k":[111]},
"Subtract":{"v":["VK_SUBTRACT"],"k":[109]},
"Separator":{"v":["VK_SEPARATOR"],"k":[108]}};
var g_mpos={x:0,y:0};
var g_kb_down={};
var g_kb_changed={};
var g_zDelta=0;
function start(){
  document.documentElement.style.overflow='hidden';
  document.body.scroll="no";
  g_keys.mbLeft={k:[257]};
  g_keys.mbRight={k:[258]};
  g_keys.mbMiddle={k:[259]};
  let mid2key=['mbLeft','mbMiddle','mbRight'];
  for(let k='a'.charCodeAt(0);k<='z'.charCodeAt(0);k++)g_keys[String.fromCharCode(k)]={k:[String.fromCharCode(k).toUpperCase().charCodeAt(0)]};
  for(let k='A'.charCodeAt(0);k<='Z'.charCodeAt(0);k++)g_keys[String.fromCharCode(k)]={k:[k]};
  for(let k='0'.charCodeAt(0);k<='9'.charCodeAt(0);k++)g_keys[String.fromCharCode(k)]={k:[k]};

  // === [Добавлено] ===
  // Быстрый обратный индекс: code -> keyname
  let g_code2key={};
  for(let keyname in g_keys){
    let arr=g_keys[keyname].v||[];
    for(let v of arr){
      g_code2key[v]=keyname;
    }
  }
  // Добавим стандартные коды для букв и цифр
  for(let c=0;c<26;c++){
    g_code2key["Key"+String.fromCharCode(65+c)] = String.fromCharCode(65+c); // "KeyA" -> "A"
  }
  for(let c=0;c<10;c++){
    g_code2key["Digit"+c]=""+c;
    g_code2key["Numpad"+c]=""+c;
  }
  // ==================

  document.addEventListener('keydown',function(event){
    let keyname=null;
    if(event.key in g_keys)keyname=event.key;
    else if(event.code && (event.code in g_code2key))keyname=g_code2key[event.code];
    switch(event.key) {
      case '+': keyname = 'Add'; break;
      case '-': keyname = 'Subtract'; break;
      case '*': keyname = 'Multiply'; break;
      case '/': keyname = 'Divide'; break;
    }
    if(keyname){
      let k=g_keys[keyname];
      k.down=true;
      k.changed=true;
      k.k.map(k=>g_kb_down[k]=1);
      k.k.map(k=>g_kb_changed[k]=1);
    }
    if(event.keyCode===8){
      event.preventDefault();
    }
  });
  document.addEventListener('keyup',function(event){
    let keyname=null;
    if(event.key in g_keys)keyname=event.key;
    else if(event.code && (event.code in g_code2key))keyname=g_code2key[event.code];
    switch(event.key) {
      case '+': keyname = 'Add'; break;
      case '-': keyname = 'Subtract'; break;
      case '*': keyname = 'Multiply'; break;
      case '/': keyname = 'Divide'; break;
    }
    if(keyname){
      let k=g_keys[keyname];
      k.down=false;
      k.changed=true;
      k.k.map(k=>g_kb_down[k]=0);
      k.k.map(k=>g_kb_changed[k]=1);
    }
  });
  document.addEventListener('mousemove',function(e){
    var x=e.pageX;
    var y=e.pageY;
    g_mpos={x,y};
  });
  function normalizeMouseButton(e) {
    var button = e.button;
    if (navigator.userAgent.indexOf("MSIE ") > -1 || navigator.userAgent.indexOf("Trident/") > -1) {
      if (button === 1) return 2;
      if (button === 2) return 1;
    }
    return button;
  }
  document.addEventListener("mousedown",(e)=>{
    if(e.button>2)return;
    let k=g_keys[mid2key[normalizeMouseButton(e)]];
    k.down=true;
    k.changed=true;
    k.k.map(k=>g_kb_down[k]=1);
    k.k.map(k=>g_kb_changed[k]=1);
    prevent_event(e);
  });
  document.addEventListener("mouseup",(e)=>{
    if(e.button>2)return;
    let k=g_keys[mid2key[normalizeMouseButton(e)]];
    k.down=false;
    k.changed=true;
    k.k.map(k=>g_kb_down[k]=0);
    k.k.map(k=>g_kb_changed[k]=1);
    prevent_event(e);
  });
  document.addEventListener('contextmenu',prevent_event);
  window.addEventListener('wheel', function(e) {
    if (e.deltaY > 0) {
      g_zDelta=e.deltaY;console.log(g_zDelta);
    } else {
      g_zDelta=e.deltaY;console.log(g_zDelta);
    }
    return prevent_event(e);
  }, {passive:false});
  function handleStart(event) {
    event.preventDefault();
    for (const changedTouch of event.changedTouches) {
      g_mpos.x=changedTouch.pageX;
      g_mpos.y=changedTouch.pageY;
    }
    let k=g_keys[mid2key[0]];
    k.down=true;
    k.changed=true;
    k.k.map(k=>g_kb_down[k]=1);
    k.k.map(k=>g_kb_changed[k]=1);
  }
  document.addEventListener("touchstart", handleStart);
  function handleMove(event) {
    event.preventDefault();
    for (const changedTouch of event.changedTouches) {
      g_mpos.x=changedTouch.pageX;
      g_mpos.y=changedTouch.pageY;
    }
  }
  document.addEventListener("touchmove", handleMove);
  function handleEnd(event) {
    event.preventDefault();
    for (const changedTouch of event.changedTouches) {
      g_mpos.x=changedTouch.pageX;
      g_mpos.y=changedTouch.pageY;
    }
    let k=g_keys[mid2key[0]];
    k.down=false;
    k.changed=true;
    k.k.map(k=>g_kb_down[k]=0);
    k.k.map(k=>g_kb_changed[k]=1);
  }
  document.addEventListener("touchend", handleEnd);
  let host=0?"185.92.223.117":document.location.host+"";
  console.log({host});
  Module.ccall('qap_main','int',["string"],[host]);
}
var prevent_event=event=>{
  event.preventDefault();
  event.stopPropagation();
  event.cancelBubble=true;
  event.returnValue=false;
  return false;
};
const fetchFile=async dataURL=>{
  return await fetch(dataURL).then(response=>response.text())
}
function fetchFile_v2(dataURL){
  let url=dataURL.split(" ").slice(1).join(" ");
  let preview=async url=>{
    let data=await fetchFile("http://"+url);
    let s=data+"";
    let p=stringToNewUTF8(s);
    console.log({p,L:s.length,s});
    let retval=Module.ccall(
      'qap_on_load_url','number',
      ['string','number','number'],
      [dataURL,p,s.length]
    );
    Module._free(p);
  };
  preview(url);
};

function initShaderProgram(gl,vsSource,fsSource){
  const vertexShader = loadShader(gl, gl.VERTEX_SHADER, vsSource);
  const fragmentShader = loadShader(gl, gl.FRAGMENT_SHADER, fsSource);
  const shaderProgram = gl.createProgram();
  gl.attachShader(shaderProgram, vertexShader);
  gl.attachShader(shaderProgram, fragmentShader);
  gl.linkProgram(shaderProgram);
  if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
    alert(
      `Unable to initialize the shader program: ${gl.getProgramInfoLog(
        shaderProgram
      )}`
    );
    return null;
  }
  return shaderProgram;
}

function loadShader(gl,type,source){
  const shader = gl.createShader(type);
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    alert(
      `An error occurred compiling the shaders: ${gl.getShaderInfoLog(shader)}`
    );
    gl.deleteShader(shader);
    return null;
  }
  return shader;
}
function bindTex(qDev,texture){
  let gl=qDev.gl;let tex=g_textures[texture];
  gl.uniform1f(qDev.prog.uniformLocations.use_tex,tex?1.0:0.0);
  if(!tex)return;
  gl.activeTexture(gl.TEXTURE0);
  gl.bindTexture(gl.TEXTURE_2D,tex);
}
g_textures=[null];
function makeTexture(qDev,w,h,pix){
  let gl=qDev.gl;
  let pixel=new Uint8Array(HEAPU8.buffer,pix,w*h*4);
  const texture=gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D,texture);
  const level = 0;
  const internalFormat = gl.RGBA;
  const width = w;
  const height = h;
  const border = 0;
  const srcFormat = gl.RGBA;
  const srcType = gl.UNSIGNED_BYTE;
  gl.texImage2D(
    gl.TEXTURE_2D,
    level,
    internalFormat,
    width,
    height,
    border,
    srcFormat,
    srcType,
    pixel
  );
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.generateMipmap(gl.TEXTURE_2D);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
  let tex_id=g_textures.length;
  g_textures.push(texture);
  return tex_id;
}
function loadTexture_v2(url){
  const img=new Image();
  img.onload=()=>{
    let c=document.createElement('canvas');
    let ctx=c.getContext('2d');
    c.width=img.width;c.height=img.height;
    ctx.drawImage(img,0,0);
    let d=ctx.getImageData(0,0,c.width,c.height).data;
    let n=c.width*c.height*4;
    let p=Module._malloc(n);
    for(let i=0;i<n;i++)HEAPU8[p+i]=d[i];
    //let ptr=allocate(intArrayFromString(url),'i8',ALLOC_NORMAL);
    //let ptr=stringToNewUTF8(url);
    let retval=Module.ccall(
      'qap_on_load_img','number',
      ['string','number','number','number'],
      [url,p,c.width,c.height]
    );
    //_free(p);  
  };
  img.src=url.split("\\").join("/");
}
function loadTexture(gl,url) {
  const texture=gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, texture);
  const level=0;
  const internalFormat=gl.RGBA;
  const width=1;
  const height=1;
  const border=0;
  const srcFormat=gl.RGBA;
  const srcType=gl.UNSIGNED_BYTE;
  const pixel=new Uint8Array([0,0,255,255]);
  gl.texImage2D(
    gl.TEXTURE_2D,
    level,
    internalFormat,
    width,
    height,
    border,
    srcFormat,
    srcType,
    pixel
  );
  const image=new Image();
  image.onload=()=>{
    gl.bindTexture(gl.TEXTURE_2D,texture);
    gl.texImage2D(
      gl.TEXTURE_2D,
      level,
      internalFormat,
      srcFormat,
      srcType,
      image
    );
    if(isPowerOf2(image.width)&&isPowerOf2(image.height)){
      gl.generateMipmap(gl.TEXTURE_2D);
    }else{
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);
    }
  };
  image.src=url;
  return texture;
}

function isPowerOf2(value) {
  return (value & (value - 1)) === 0;
}

// Размер буфера для передачи в WASM (можно подстроить)
const CHUNK_SIZE = 64 * 1024; // 64KB

async function streamProcessReplay(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error('Failed to fetch ' + url);
  }

  const reader = response.body.getReader();

  // Буфер внутри JS для накопления данных перед передачей в WASM
  let buffer = new Uint8Array(CHUNK_SIZE);
  let bufferOffset = 0;

  for(;;) {
    const { done, value } = await reader.read();
    if (done) {
      // Передать оставшиеся данные в WASM
      if (bufferOffset > 0) {
        processChunk(buffer.subarray(0, bufferOffset));
      }
      Module.ccall('process_replay_end', null, [], []);
      console.log('Stream finished');
      break;
    }

    let start = 0;
    while (start < value.length) {
      // Сколько памяти осталось в буфере
      const spaceLeft = CHUNK_SIZE - bufferOffset;
      const toCopy = Math.min(spaceLeft, value.length - start);

      buffer.set(value.subarray(start, start + toCopy), bufferOffset);
      bufferOffset += toCopy;
      start += toCopy;

      if (bufferOffset === CHUNK_SIZE) {
        processChunk(buffer);
        bufferOffset = 0;
      }
    }
  }
}
function processChunk(chunk) {
  console.log("chunk.length",chunk.length);
  const ptr = Module._malloc(chunk.length);
  HEAPU8.set(chunk, ptr);
  console.log("processChunk",ptr,chunk.length);
  console.log("chunk.process_replay_chunk::bef");
  Module.ccall('process_replay_chunk', null, ['number', 'number'], [ptr, chunk.length]);
  console.log("chunk.process_replay_chunk::aft");
  Module._free(ptr);
  console.log("chunk.process_replay_chunk::free");
}
function start_replay(){
  let h=document.location.hash;
  if(h.length>0)h=h.substr(1);
  h=h.length?h:0;
  var srv=document.location.protocol+"//"+document.location.host.split(":")[0];
  streamProcessReplay(srv+':3000/stream/'+h).catch(console.error);
}