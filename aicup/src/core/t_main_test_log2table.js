var FToS=n=>(n+0).toFixed(2);
var mapswap=(k2v)=>{var v2k={};for(var k in k2v){v2k[k2v[k]]=k;}return v2k;}
var qapavg=(arr,cb)=>{if(typeof cb=='undefined')cb=e=>e;return arr.length?arr.reduce((pv,ex)=>pv+cb(ex),0)/arr.length:0;}
var qapsum=(arr,cb)=>{if(typeof cb=='undefined')cb=e=>e;return arr.reduce((pv,ex)=>pv+cb(ex),0);}
var qapmin=(arr,cb)=>{if(typeof cb=='undefined')cb=e=>e;var out;var i=0;for(var k in arr){var v=cb(arr[k]);if(!i){out=v;}i++;out=Math.min(out,v);}return out;}
var qapmax=(arr,cb)=>{if(typeof cb=='undefined')cb=e=>e;var out;var i=0;for(var k in arr){var v=cb(arr[k]);if(!i){out=v;}i++;out=Math.max(out,v);}return out;}
var qapsort=(arr,cb)=>{if(typeof cb=='undefined')cb=e=>e;return arr.sort((a,b)=>cb(b)-cb(a));}
var mapdrop=(e,arr,n)=>{var out=n||{};Object.keys(e).map(k=>arr.indexOf(k)<0?out[k]=e[k]:0);return out;}
var mapsort=(arr,cb)=>{if(typeof cb=='undefined')cb=(k,v)=>v;var out={};var tmp=qapsort(mapkeys(arr),k=>cb(k,arr[k]));for(var k in tmp)out[tmp[k]]=arr[tmp[k]];return out;}
var table_fix_fields=arr=>{var n2keys=[];arr.map(e=>n2keys[mapkeys(e).length]=mapkeys(e));var order=n2keys.pop();return arr.map(e=>{var m={};order.map(k=>m[k]=k in e?e[k]:0);return m;});};

var qap_unique=arr=>{var tmp={};arr.map(e=>tmp[e]=1);return mapkeys(tmp);};var unique_arr=qap_unique;

var mapaddfront=(obj,n)=>{for(var k in obj)n[k]=obj[k];return n;}
var mapclone=obj=>mapaddfront(obj,{});

var getarr=(m,k)=>{if(!(k in m))m[k]=[];return m[k];};
var getmap=(m,k)=>{if(!(k in m))m[k]={};return m[k];};
var getdef=(m,k,def)=>{if(!(k in m))m[k]=def;return m[k];};
//next: fastlog_parser
let fs=require("fs");
var POST={data:fs.readFileSync("../x64/Release/NN2.log")+""};
//console.log(POST.data.length);
var S1="=== META STRATEGIES TOP (current: S1) ===";
var R1="=== META STRATEGIES TOP (current: R1) ===";
var R2="=== META STRATEGIES TOP (current: R2) ===";
var F="=== META STRATEGIES TOP (current: F) ===";
var meta=F;var metaend="Activated";
var arr=POST.data.split("\r").join("").split(meta+"\n");
arr=arr.slice(1);let oarr=[];let name2info={};
console.log(POST.data.length,arr.length);
for(let i=0;i<arr.length;i+=meta==F?2:1){
  let a=arr[i].split(meta==F?"Phase":metaend)[0].split("\n");
  let out=[];
  a.map(e=>{
    var t=e.split(" ");
    if(t.length<6)return "";
    var n=t.length;let g=t;
    if(n==8)g=[t[0],t[1]+" "+t[2],t[3],t[4],t[5],t[6],t[7]];
    t=g;
    let obj={src:t[6],score:t[5]*1.0,name:t[1]};
    let rec=getdef(name2info,obj.name,{src:t[6],name:obj.name,n:0,sum:0,max:-1e99,min:+1e99});
    //rec.log.push(t);
    rec.n++;rec.sum+=obj.score;rec.avg=rec.sum/rec.n;rec.max=Math.max(rec.max,obj.score);rec.min=Math.min(rec.min,obj.score);
    out.push(JSON.stringify(obj));
  });
  oarr.push(out.join("\n"));
  //break;
}
let m=[];for(let k in name2info){m.push(name2info[k]);}
qapsort(m,e=>e.avg);
//console.log(Object.keys(name2info));
fs.writeFileSync("out.log.js","return html(csv2table_v3(maps2csv("+JSON.stringify(m,0,2)+")));;");
/*
return html(csv2table_v3(maps2csv(m)));//JSON.stringify(name2info,0,2));
return oarr.join("\n\n");
//next: genpack_extractor
return POST.data.split("\n").map(e=>{
  var t=e.split(" ");
  var ai=t[t.length-1];
  var a=t[1].split("genpack").join("K");//e.split("UID=")[1].split(" ")[0];
  return ((a|0)<0)||((a|0)>218)?"":"F("+ai+',"'+a+'");';
}).filter(e=>e.length).join("\n");
*/