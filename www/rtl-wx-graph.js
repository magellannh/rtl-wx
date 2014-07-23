/* This code is based on original work by an unknown author and was
heavily modified for use with FreeWX and FreeWX-Wi in 2005.
Andy Keir, March 4 2005 7:00pm.

From Jan 1 2006 David Brown has further heavily modified the code to 
fix some issues and to ensure compatibility with the HTML 4 
specification as well as improve performance and functionality.
**********************************************************************
Last Modified 02 Feb 06 at 11:03 - DNB (Burger).
Version: 2.00.02 Final
**********************************************************************
jsgraphit@dandmbr.co.uk - http://www.dandmbr.co.uk/jsgraphit/
**********************************************************************
NOTE: IMPORTANT CHANGE FROM VERSION 2 BUILD 240106 B BETA.
Version 1 user settings have been removed from the script
but can still be made by setting them in the HTML.
See http://www.dandmbr.co.uk/weather/gtest/v2FWXStdGuide.htm
*********************************************************************/
var tdstyle="font-size: 1px; margin: 0px; padding: 0px; border-width: 0px; line-height: 0px;";
var im="<DIV style='width:1px;height:1px;overflow:hidden;'></DIV>";
var doLines,doLSize,gtf,gts,gtc,gaf,gas,gac,wdcp,wdgl,wdgls,wdbi,negline,gtext;
function setdefault(g){
if(window.gtitlefont) gtf=gtitlefont; else gtf="Verdana,Arial,Helvetica";
if(window.gtitlesize) gts=gtitlesize; else gts="10";
if(window.gtitlecolor) gtc=gtitlecolor; else gtc="";
if(window.gaxisfont) gaf=gaxisfont; else gaf="Arial,Helvetica";
if(window.gaxissize) gas=gaxissize; else gas="10";
if(window.gaxiscolor) gac=gaxiscolor; else gac="";
if(window.wdcompass) wdcp=wdcompass; else wdcp=1;
if(window.wdgraphline) wdgl=wdgraphline; else wdgl=1;
if(window.wdglsize) wdgls=wdglsize; else wdgls=2;
if(window.wdbarimage) wdbi=wdbarimage; else wdbi="rtl-wx-graph-bar.gif";
if(!window.negline) negline=0;
if(!window.gwidth) gwidth=0;
if(!window.gheight) gheight=0;
if(!g.barcolors){ g.barcolors=new Array(); if(window.barimage) g.barcolors[0]=barimage; else g.barcolors[0]="rtl-wx-graph-bar.gif"; }
if(!g.negcolors){ g.negcolors=new Array(); if(window.negbarimage) g.negcolors[0]=negbarimage; else g.negcolors[0]="rtl-wx-graph-bar.gif"; }
doLines=0; 
doLSize=1;
}
function Graph(width,height,backcol,offset,IsWind,NonMetric,time24,ext,res){
setdefault(this);
this.res=res;
this.ext=ext;
this.backcol=backcol;
this.offset=offset;
this.IsWind=IsWind;
this.NonMetric=NonMetric;
this.width=gwidth || width || 400;
this.height=gheight || height || 200;
this.setBarcolors=setBcls;
this.setNegcolors=setNcls;
this.rows=new Array();
this.addRow=addRG;
this.setXScale=setXSG;
this.setXScaleValues=setXSVG;
this.setTime=setSTG;
this.setDate=setSDG;
this.build=buildG;
this.setLegend=setLG;
this.writeLegend=writeLG;
this.time24=time24;
this.setTitles=setTS;
this.setAxis=setAS;
this.setWindoptions=setWO;
this.setLines=setLs;
return this;
}
function setWO(){
if(arguments){
if(arguments[0]) wdcp=arguments[0];
if(arguments[1]) wdgl=arguments[1];
if(arguments[2]) wdgls=arguments[2];
if(arguments[3]) wdbi=arguments[3];
}
}
function setAS(){
if(arguments){
if(arguments[0]) gaf=arguments[0];
if(arguments[1]) gas=arguments[1];
if(arguments[2]) gac=arguments[2];
}
}
function setTS(){
if(arguments){
if(arguments[0]) gtf=arguments[0];
if(arguments[1]) gts=arguments[1];
if(arguments[2]) gtc=arguments[2];
}
}
function setLs(){
if(arguments){
if(arguments[0]) doLines=arguments[0];
if(arguments[1]) doLSize=arguments[1];
}
}
function setGs(g){
if(g.IsWind==1){ g.scale=45; return; }
ma=0, mi=10000;
for(i=0; i<g.icount; i++){
for(j=0; j<g.jcount; j++){
tn=g.rows[i][j];
if(tn>ma) ma=tn;
if(tn<mi) mi=tn;
}
}
if(mi==10000) mi=0;
g.scale=Math.max(1, parseInt(Math.ceil((ma-mi+1)/(g.height/15))));
if(g.ext !="external") g.negbars=negline;
if(mi<0 && g.negbars==1){
g.offset=0;
} else if(mi !=0){
g.offset=parseInt(Math.ceil(mi))-1;
}else{
g.offset=0;
}
}
function setBcls(){this.barcolors=arguments;}
function setNcls(){this.negcolors=arguments;}
function setLG(){this.legends=arguments;}
function addRG(){ 
ic=arguments.length;
this.rows[this.rows.length]=new Array();
var row=this.rows[this.rows.length-1];
for(var i=0; i<ic; i++){
if(arguments[i]== -999){
row[row.length]="NaN";
}else{
if(this.NonMetric==1) row[row.length]=(arguments[i]*100);
else row[row.length]=arguments[i];
}
}
this.icount=this.rows.length;
this.jcount=this.rows[0].length;
}
function rescaleG(g){
g.posMax=0, g.negMax=0, g.c=0;
for(var i=0; i<g.icount; i++){
for(var j=0; j<g.jcount; j++){
g.c++;
if(g.rows[i][j]>g.posMax) g.posMax=g.rows[i][j];
if(g.rows[i][j]<-100) g.rows[i][j]=g.negMax;
if(g.rows[i][j]<g.negMax) g.negMax=g.rows[i][j];
}
}
if(g.NonMetric==0){
if(g.posMax<1 && g.posMax>=0) g.posMax=1;
if(g.negMax>-1 && g.negMax<0) g.negMax=-1;
}
if(g.NonMetric==1){
if(g.posMax<3 && g.posMax>=0) g.posMax=3;
if(g.negMax>-3 && g.negMax<0) g.negMax=-3;
}
if(g.IsWind==1){
g.posMax=360;
g.negMax=0;
}
g.vscale=g.height/(g.posMax-g.negMax);
g.hscale=Math.floor(g.width/g.c-1/g.jcount);
}
function stab(){if(window.shtab) return 1;return 0;}
function writeLG(){
var st="";
st+="<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0><TR><TD><TABLE BORDER="+stab()+" CELLSPACING=0 CELLPADDING=2><TR><TD>";
for(var i=0; i<this.legends.length; i++){
if(!this.legends[i]) continue;
if(i>=this.icount) break;
if(this.barcolors) gifsrc=this.barcolors[i];
else gifsrc="#FF0000";
st+="<DIV style='font-family:"+gaf+";font-size:"+gas+"px;background-color:"+gifsrc+";'>&nbsp;&nbsp;</DIV></TD><TD>";
st+="<DIV style='font-family:"+gaf+";font-size:"+gas+"px;color:"+gac+";'>"+this.legends[i]+"</DIV>";
if(i<this.legends.length -1) st+="</TD></TR><TR><TD>";
}
st+="</TD></TR></TABLE></TD></TR></TABLE>";
return st;
}
function checkT(gt){
switch(gt){
case("Temperature: last 24 hours"):if(window.ott)if(ott !=""){gt=ott};break;
case("Humidity: last 24 hours"):if(window.oht)if(oht !=""){gt=oht};break;
case("Barometer: last 24 hours"):if(window.bt)if(bt !=""){gt=bt};break;
case("Dew Point: last 24 hours"):if(window.dpt)if(dpt !=""){gt=dpt};break;
case("Avg. wind speed: last 24 hours"):if(window.awt)if(awt !=""){gt=awt};break;
case("Wind direction: last 24 hours"):if(window.wdt)if(wdt !=""){gt=wdt};break;
case("Rainfall: last 24 hours"):if(window.rft)if(rft !=""){gt=rft};break;
case("Indoor temperature: last 24 hours"):if(window.itt)if(itt !=""){gt=itt};break;
case("Indoor humidity: last 24 hours"):if(window.iht)if(iht !=""){gt=iht};break;
}
return(gt);
}
function checkL(gt,gl){
switch(gt){
case("Temperature: last 24 hours"):if(window.otl)if(otl !=""){gl=otl};break;
case("Humidity: last 24 hours"):if(window.ohl)if(ohl !=""){gl=ohl};break;
case("Barometer: last 24 hours"):if(window.bl)if(bl !=""){gl=bl};break;
case("Dew Point: last 24 hours"):if(window.dpl)if(dpl !=""){gl=dpl};break;
case("Avg. wind speed: last 24 hours"):if(window.awl)if(awl !=""){gl=awl};break;
case("Wind direction: last 24 hours"):if(window.wdl)if(wdl !=""){gl=wdl};break;
case("Rainfall: last 24 hours"):if(window.rfl)if(rfl !=""){gl=rfl};break;
case("Indoor temperature: last 24 hours"):if(window.itl)if(itl !=""){gl=itl};break;
case("Indoor humidity: last 24 hours"):if(window.ihl)if(ihl !=""){gl=ihl};break;
}
if(gl=="n"){gl="&nbsp;&nbsp;";}
return(gl);
}
function setBar (s,g,i,j,h,w,axis,maxvl,minvl){
var stb="";
var bstyle="";
switch(axis){
case(1):if(g.setPosStyle) bstyle=g.setPosStyle;break;
case(2):if(g.setNegStyle) bstyle=g.setNegStyle;break;
case(3):if(g.setMidStyle) bstyle=g.setMidStyle;break;
}
stb+="<div ";
stb+="title='"; 
if(!g.mmg){
if(g.dates) stb+=g.dates[j]+": "; 
if(g.legends && g.legends[i]) stb+=g.legends[i]+": "; 
if(g.NonMetric==1) stb+=Math.round(g.rows[i][j]+g.offset)/100;
else stb+=Math.round((g.rows[i][j]+g.offset)*100)/100;
}else{
if(g.dates) stb+=g.dates[j]+": "; 
if(g.NonMetric==1) stb+=Math.round(minvl+g.offset)/100+" to "+Math.round(maxvl+g.offset)/100;
stb+=Math.round((minvl+g.offset)*100)/100+" to "+Math.round((maxvl+g.offset)*100)/100;
}
if(g.yLabel) stb+=" "+g.yLabel.split("<BR>\n").join(" ");
stb+="' ";
if(g.IsWind && wdbi!="") bcol=wdbi;
else if(g.barcolors && s==1) bcol=g.barcolors[i];
else if(g.barcolors && s==-1) bcol=g.negcolors[i];
else bcol="#FF0000";
stb+="style='position:relative;cursor:pointer;overflow:hidden;height:"+h+"px;width:"+w+"px;"+bstyle;
if(g.g3d=="in"){
stb+="border-style:solid;border-color:#222222 #DDDDDD #DDDDDD #222222;border-width:1px;";
} else if(g.g3d=="out"){
stb+="border-style:solid;border-color:#DDDDDD #222222 #222222 #DDDDDD;border-width:1px;";
}
if(bcol.lastIndexOf(".")>0){
stb+="'><IMG SRC="+bcol+" WIDTH="+w+" HEIGHT="+h+" BORDER=0>";
}else if(bcol!=""){ 
stb+="background-color:"+bcol+";layer-background-color:"+bcol+"'>";
}
stb+="</div>";
return(stb);
}
function buildRG(g, doc){
var tb=stab();
var threed=0;
var fw=0;
var dLs=0;
if(doLines && window.regex) dLs=1;
if(g.g3d) threed=2;
if(g.MaxMinGraph==1 && g.icount==2) g.mmg=1;
if(g.gBorderWidth) fw=g.gBorderWidth;
var str="";
if(g.bgColor){
if(g.bgColor.lastIndexOf(".")>0){
if(g.gBorder) str+="<TABLE width="+fw+" BORDER="+g.gBorder+" CELLPADDING=5 CELLSPACING=0 style='background-image:url(\""+g.bgColor+"\");'><TR><TD align=center>\n";
else str+="<TABLE width="+fw+" BORDER=0 CELLPADDING=5 CELLSPACING=0 style='background-image:url(\""+g.bgColor+"\");'><TR><TD align=center>\n";
}else{
if(g.gBorder) str+="<TABLE width="+fw+" BORDER="+g.gBorder+" CELLPADDING=5 CELLSPACING=0><TR><TD BGCOLOR="+g.bgColor+" align=center>\n";
else str+="<TABLE width="+fw+" BORDER=0 CELLPADDING=5 CELLSPACING=0><TR><TD BGCOLOR="+g.bgColor+" align=center>\n";
}
} 
else if(g.gBorder) str+="<TABLE BORDER="+g.gBorder+" CELLPADDING=5 CELLSPACING=0><TR><TD>\n";
str+="<TABLE BORDER="+tb+" CELLPADDING="+(tb*2)+" CELLSPACING=0>\n";
if(g.title){
str+="<TR>\n";
cs=g.c;
if(g.gBorder || g.bgColor){ if(g.scale) cs+=3; if(g.yLabel) cs+=1; if(g.legends) cs+=2; 
str+="<TH VALIGN=TOP HEIGHT=20 COLSPAN="+(cs)+">\n"; }
else{
if(g.scale) str+="<TD COLSPAN=3 HEIGHT=20>"+im+"</TD>\n";
if(g.yLabel) str+="<TD>"+im+"</TD>\n";
str+="<TH VALIGN=TOP HEIGHT=20 COLSPAN="+(g.c)+">\n";
}
str+="<DIV style='font-family:"+gtf+";font-size:"+gts+"px;color:"+gtc+";'>";
str+=checkT(g.title);
str+="</DIV></TH>";
if(g.legends && !g.gBorder && !g.bgColor) str+="<TD COLSPAN=2>"+im+"</TD>";
str+="</TR>\n";
}
if(g.yLabel){
g.yLabel=g.yLabel.split(" ");
g.yLabel=g.yLabel.join("<BR>\n");
str+="<TR>\n";
var r=2; if(g.negMax && g.posMax) r++;
str+="<TH ROWSPAN="+r+" ALIGN=LEFT WIDTH=20 NOWRAP>\n";
str+="<DIV style='padding:2px;font-family:"+gtf+";font-size:"+gts+"px;color:"+gtc+";'>"+checkL(g.title,g.yLabel)+"</DIV></TD>\n";
}
var wdth=parseInt(g.hscale);
if(g.posMax>0){
if(dLs) talign="top";
else talign="bottom";
if(!g.yLabel) str+="<TR>\n";
if(g.scale) str+=writeSG(g, 0, g.posMax); 
if(!g.mmg==1) str+="<TD VALIGN="+talign+" align=left width="+(g.hscale+1)+" style='"+tdstyle+"'>";
else str+="<TD VALIGN=BOTTOM width="+((wdth*2)+2)+" style='"+tdstyle+"'>";
var colcount=1;
var barseg=g.vscale+g.ptoadd;
var je=g.jcount;
var ie=g.icount;
if(dLs){
linesw=(g.icount*g.jcount)*(g.hscale+1);
linesh=g.paxisheight+g.naxisheight+16;
str+="<DIV ID='"+doLines+"' style='width:"+(linesw)+"px;position:absolute;height:"+linesh+"px;'></div>";
}
for(var j=0; j<je; j++){
for(var i=0; i<ie; i++){
if((g.vscale*g.rows[i][j])>0){
if(g.mmg==1){
if(i==0){
maxval=Math.max(g.rows[0][j],g.rows[1][j]);
minval=Math.min(g.rows[0][j],g.rows[1][j]);
maxvallabel=maxval;
minvallabel=minval;
if(g.offset==0 && minval<0) minval=0;
str+="<Table cellpadding=0 cellspacing=0 border="+tb+" height="+parseInt(barseg*maxval)+" width="+(wdth*2)+">";
str+="<tr><td valign=top>";
if(g.offset !=0) str+=setBar(1,g,i,j,parseInt((barseg*maxval)-(barseg*minval))-threed || 1,((wdth*2)+1)-threed,3,maxvallabel,minvallabel);
else str+=setBar(1,g,i,j,parseInt((barseg*maxval)-(barseg*minval))-threed || 1,((wdth*2)+1)-threed,1,maxvallabel,minvallabel);
str+="</td></tr></table>";	   
}
} else if(g.IsWind && wdgl==1){
str+="<Table cellpadding=0 cellspacing=0 border="+tb+" height="+parseInt(g.vscale*g.rows[i][j])+" width="+(wdth+1)+">";
str+="<tr><td valign=top>";
str+=setBar(1,g,i,j,wdgls-threed,wdth-threed,0);
str+="</td></tr></table>";
}else{
if(!dLs) str+=setBar(1,g,i,j,parseInt(barseg*g.rows[i][j])-threed || 1,wdth-threed,1);
else str+=im;
}
}else{
if(!g.mmg || (g.mmg==1 && i==0)) str+=im; 
} 
if(!g.mmg==1){
str+="</TD>\n";
if(colcount<je*g.icount) str+="<TD VALIGN=BOTTOM WIDTH="+(g.hscale+1)+" style='"+tdstyle+"'>";
}else{
if(i==0){
str+="</TD>\n";
if(colcount<je*ie-1) str+="<TD VALIGN=BOTTOM width="+((wdth*2)+2)+" style='"+tdstyle+"'>";
}
}
colcount++;
}  
} 
if(j<g.jcount) str+="</TD>\n";
} 
if(g.legends) str+="<TD WIDTH=10 NOWRAP ROWSPAN=3>"+im+"</TD><TD ROWSPAN=3>"+g.writeLegend()+"</TD>\n"
if(g.scale || g.xScale){
if(g.posMax) str+="</TR><TR HEIGHT=1>\n";
else str+="</TR><TR HEIGHT=1><TD COLSPAN=2 style='"+tdstyle+" height: 1px'>"+im+"</TD>\n";
str+="<TD VALIGN=BOTTOM ALIGN=LEFT COLSPAN="+(g.c+1)+" style='"+tdstyle+" height: 1px'>";
str+="<DIV style='width:"+parseInt((je*(g.hscale+1)*ie)+2);
str+="px;height:1px;overflow:hidden;background-color:"+g.backcol+";'></DIV>";
str+="</TD></TR>\n";
}
if(g.xScale && !g.negMax) str+=writeXSG(g);
if(g.negMax<0){
if(g.posMax !=0 && !g.scale) str+="</TR>";
str+="<TR>\n";
if(g.scale) str+=writeNSG(g, g.negMax, 0);
if(!g.mmg==1) str+="<TD VALIGN=TOP width="+(g.hscale+1)+" style='"+tdstyle+"'>";
else str+="<TD VALIGN=TOP width="+((wdth*2)+2)+" style='"+tdstyle+"'>";
var colcount=1;
var barseg=(g.vscale+g.ntoadd)*-1;
var je=g.jcount;
var ie=g.icount;
for(var j=0; j<je; j++){
for(var i=0; i<ie; i++){
if(parseInt(g.vscale*g.rows[i][j])<0 || g.mmg==1){
if(g.mmg==1){
if(i==0){
maxval=Math.max(g.rows[0][j],g.rows[1][j]);
minval=Math.min(g.rows[0][j],g.rows[1][j]);
maxvallabel=maxval;
minvallabel=minval;
if(g.offset==0 && maxval>0) maxval=0;
h=maxval+minval;
if(parseInt(g.vscale*-h)>0){
str+="<Table cellpadding=0 cellspacing=0 border="+tb+" height="+parseInt(minval*g.vscale*-1)+" width="+((wdth)*2)+">";
str+="<tr><td valign=bottom>";
str+=setBar(-1,g,i,j,parseInt((minval-maxval)*barseg)-threed || 1,((wdth*2)+1)-threed,2,maxvallabel,minvallabel);
str+="</td></tr></table>";
}else{
str+=im;
}	   
}
}else{
if(!dLs) str+=setBar(-1,g,i,j,parseInt(barseg*g.rows[i][j])-threed,wdth-threed,2);
else str+=im;
}
}else{
str+=im;
}
if(!g.mmg==1){
str+="</TD>\n";
if(colcount<je*ie) str+="<TD VALIGN=TOP width="+(g.hscale+1)+" style='"+tdstyle+"'>";
}else{
if(i==0){
str+="</TD>\n";
if(colcount<je*ie-1) str+="<TD VALIGN=TOP width="+((wdth*2)+2)+" style='"+tdstyle+"'>";
}
}
colcount++;
} 
} 
str+="</TD>\n";
} 
str+="</TR>\n";
if(g.negbars && g.shownegscale && g.negMax<0){
if(g.yLabel) str+="</TR><TR HEIGHT=1><TD COLSPAN=3 style='"+tdstyle+" height: 10px'>"+im+"</TD>\n";
else str+="</TR><TR HEIGHT=1><TD COLSPAN=2 style='"+tdstyle+" height: 10px'>"+im+"</TD>\n";
str+="<TD VALIGN=BOTTOM ALIGN=LEFT COLSPAN="+(g.c+1)+" style='"+tdstyle+" height: 10px'>";
str+="<DIV style='width:"+parseInt((je*(g.hscale+1)*ie)+2);
str+="px;height:1px;overflow:hidden;background-color:"+g.backcol+";'></DIV>";
if(g.legends) str+="<TD colspan=2>"+im+"</TD></TR>\n"
else str+="</TD></TR>\n";
str+=writeXSG(g);
}
if(g.xLabel){
cs=g.c;
str+="<tr>";
if(g.gBorder || g.bgColor){ if(g.scale) cs+=4; if(g.yLabel) cs+=1; if(g.legends) cs+=2; 
str+="<TH HEIGHT=20 COLSPAN="+cs+">\n"; }
else str+="<td colspan=4>"+im+"</td><th COLSPAN="+(g.c)+" HEIGHT=20>\n";
str+="<DIV style='font-family:"+gtf+";font-size:"+gts+"px;color:"+gtc+";'>";
str+=g.xLabel;
str+="</div></th>";
if(g.legends && !g.gBorder && !g.bgColor) str+="<TD COLSPAN=2>"+im+"</TD>";
str+="</tr>";
}
if(g.warning){
str+="<tr><td colspan=4>"+im+"</td><td COLSPAN="+(g.c)+">\n";
str+="<table cellpadding=3 cellspacing=0 border=0 width=100%><tr><td>";
str+="<table cellpadding=3 cellspacing=0 border=1 width=100% bgcolor=#FF0000><tr><th>";
str+="<DIV style='font-family:"+gtf+";font-size:"+gts+"px;color:"+gtc+";'>";
str+="'Y' Scale too big to display units!<BR>Reduce the axis font size or increase the graph height.";
str+="</div></th></tr></table></td></tr></table></td>";
if(g.legends) str+="<TD COLSPAN=2>"+im+"</TD>";
str+="</tr>";
}
str+="</TABLE>\n";
if(g.bgColor || g.gBorder) str+="</TD></TR></TABLE>\n";
if(!g.res)doc.write(str); else gtext=str;
if(dLs && !g.res) doLineGraph(g, doc);
}
function doLineGraph(g, doc){
linesh=g.paxisheight+g.naxisheight+16;
var jg=new jsGraphics(doLines);
var dlypoints=new Array();
var dlxpoints=new Array();
for(i=0; i<g.icount; i++){
dlypoints[i]=new Array();
dlxpoints[i]=new Array();
var st="";
for(j=0; j<g.jcount; j++){
if(!isNaN(g.rows[i][j])){
dlxpoints[i][j]=(((g.hscale*g.icount)+g.icount)*j);
if(g.negMax<0){
if(g.rows[i][j]>=0){
barseg=g.vscale+g.ptoadd;
valh = parseInt(barseg*g.rows[i][j]);
dlypoints[i][j]=(g.paxisheight+15)-valh;
}else{
barseg=(g.vscale+g.ntoadd)*-1;
valh = parseInt(barseg*g.rows[i][j]);
dlypoints[i][j]=(g.paxisheight+15)+valh;
}
}else{
barseg=g.vscale+g.ptoadd;
valh = parseInt(barseg*g.rows[i][j]);
dlypoints[i][j]=linesh-valh-1;
}
st+="<div style='position:absolute;top:"+(dlypoints[i][j]-3)+"px;left:"+(dlxpoints[i][j]-3);
st+="px;height:6px;width:6px;background-color:null;cursor:pointer'";
st+=" title='"; 
if(g.dates) st+=g.dates[j]+": "; 
if(g.legends && g.legends[i]) st+=g.legends[i]+": "; 
if(g.NonMetric==1) st+=Math.round(g.rows[i][j]+g.offset)/100;
else st+=Math.round((g.rows[i][j]+g.offset)*100)/100;
if(g.yLabel) st+=" "+g.yLabel.split("<BR>\n").join(" ");
st+="'></div>";
}
}
jg.setColor(g.barcolors[i]);
jg.setStroke(doLSize);
jg.drawPolyline(dlxpoints[i],dlypoints[i]);
jg.paint();
document.getElementById(doLines).innerHTML+=st;
document.getElementById(doLines).style.zIndex=1;
}
}
function setXSG(s, skip, inc){
this.xScale=true;
this.s=s || 0;
this.skip=skip || 1;
this.inc=inc || 1;
}
function setXSVG(){
this.xScale=true;
this.s=0;
this.skip=1;
this.inc=1;
this.dates=new Array();
for(var i=0; i<arguments.length; i++)
this.dates[this.dates.length]=arguments[i];
}
function setSTG(hour, min, skip, inc){
this.xScale=true;
this.sTime=new Date(0, 0, 0, hour, min);
this.skip=skip || 12;
this.inc=inc || 30;
}
function setSDG(month, day, year, skip, inc){
this.xScale=true;
this.sDate=new Date(year, month-1, day);
this.skip=skip || 1;
this.inc=inc || skip || 1;
this.showDate=true;
}
function setDAG(g){
if(g.dates) return;
g.dates=new Array();
for(var i=0; i<g.jcount; i++){
var dateStr="";
if(g.sDate){
if(g.showDay){
eval('switch(g.sDate.getDay()){'+'case 0: dateStr+="Sun"; break;'+'case 1: dateStr+="Mon"; break;'+'case 2: dateStr+="Tue"; break;'+'case 3: dateStr+="Wed"; break;'+'case 4: dateStr+="Thu"; break;'+'case 5: dateStr+="Fri"; break;'+'case 6: dateStr+="Sat"; break;'+'}'
)
dateStr+=" ";
}
if(g.longDate && g.showDate){
dateStr+=g.sDate.getDate()+"-";
eval('switch(g.sDate.getMonth()){'+'case 0: dateStr+="Jan"; break;'+'case 1: dateStr+="Feb"; break;'+'case 2: dateStr+="Mar"; break;'+'case 3: dateStr+="Apr"; break;'+'case 4: dateStr+="May"; break;'+'case 5: dateStr+="Jun"; break;'+'case 6: dateStr+="Jul"; break;'+'case 7: dateStr+="Aug"; break;'+'case 8: dateStr+="Sep"; break;'+'case 9: dateStr+="Oct"; break;'+'case 10: dateStr+="Nov"; break;'+'case 11: dateStr+="Dec"; break;'+'}'
);
} else if(g.showDate){
if(new Date("27/12/2004").getDay()==0) dateStr+=g.sDate.getDate()+"/"+(g.sDate.getMonth()+1);
else dateStr+=(g.sDate.getMonth()+1)+"/"+g.sDate.getDate();
}
if(g.showYear && g.showDate){
if(g.longDate) dateStr+="-";
else dateStr+="/";
}
if(g.showYear){
if(g.longYear) dateStr+=g.sDate.getFullYear();
else dateStr+=(g.sDate.getFullYear()%100);
}
g.sDate.setDate(g.sDate.getDate()+g.inc);
} else if(g.sTime){
var hrs=g.sTime.getHours();
if(!g.time24){
var pm=false;
if(hrs==0){ hrs=12; }
else if(hrs>=12){ if(hrs>12) hrs -=12; pm=true; }
} else 
if(hrs<10) hrs="0"+hrs;
dateStr=hrs+":";
var min=g.sTime.getMinutes();
if(min<10) min="0"+min;
dateStr+=min;
if(!g.time24){ !pm ? dateStr+="am" : dateStr+="pm" ; }
g.sTime.setMinutes(g.sTime.getMinutes()+g.inc); 
} else dateStr=g.s+i*g.inc;
g.dates[i]=dateStr;
}
}
function writeXSG(g){ 
var st="";
if(!g.c) g.c=g.jcount*2-1;
st+="<TR>\n";
if(g.scale) st+="<TD COLSPAN=2>"+im+"</TD>\n";
if(g.yLabel) st+="<TD>"+im+"</TD>\n";
if(g.MaxMinGraph==1 && g.icount==2){
var cspan=1;
}else{
var cspan=g.icount;
}
cspan*=g.skip;
var t=0;
for(var i=0; i<Math.ceil(g.jcount/g.skip); i++){
if((g.jcount*g.icount)-t<cspan) cspan=(g.jcount*g.icount)-t;
st+="<TD VALIGN=TOP ALIGN=LEFT";
st+=" COLSPAN="+cspan; t+=cspan;
st+=" style='"+tdstyle+"'><DIV style='width:1px;height:10px;overflow:hidden;background-color:"+g.backcol+";'></DIV>";
st+="</TD>\n";
if(i==0) st+="<TD>"+im+"</TD>\n";
}
if(g.legends && g.negbars && g.shownegscale) st+="<TD COLSPAN=2>"+im+"</TD>";
st+="</TR><TR>\n";
if(g.scale) st+="<TD COLSPAN=3>"+im+"</TD>\n";
if(g.yLabel) st+="<TD>"+im+"</TD>\n";
if(g.sDate || g.sTime) setDAG(g);
var t=0;
if(g.MaxMinGraph==1 && g.icount==2){
var cspan=1;
}else{
var cspan=g.icount;
}
cspan *=g.skip;
for(var i=0; i<Math.ceil(g.jcount/g.skip); i++){
if((g.jcount*g.icount)-t<cspan) cspan=(g.jcount*g.icount)-t;
var thislabel=(g.dates[i*g.skip]);
if(!isNumber(thislabel)) thislabel=thislabel.split(" ").join("<BR>\n");
if(!isNumber(thislabel)) thislabel=thislabel.split("-").join("<BR>\n");
st+="<TD VALIGN=TOP ALIGN=LEFT";
st+=" COLSPAN="+cspan; t+=cspan;
st+=" style='font-family:"+gaf+";font-size:"+gas+"px;color:"+gac+";'>";
st+=thislabel || "";
st+="</TD>\n";
}
if(g.legends) st+="<TD COLSPAN=2>"+im+"</TD>";
st+="</TR>\n"; 
return st;
}
function isNumber(value){
if(value=="") return false;
var d=parseInt(value);
if(!isNaN(d)) return true; else return false;		
}
function writeNSG(g, min, max){
var h=Math.ceil(g.height/(g.posMax-g.negMax)*g.scale);
var p=-1*g.negMax/(g.posMax-g.negMax);
var n=Math.floor((-1*g.negMax)/g.scale);
var st="";
if(h<15){
st+="<TD>"+im+"</TD><TD>"+im+"</TD><TD>"+im+"</TD>\n";
return st;
}
st+="<TD VALIGN=TOP ALIGN=RIGHT>";
st+="<TABLE BORDER="+stab()+" CELLSPACING=0 CELLPADDING=0><TR><TD VALIGN=BOTTOM HEIGHT="+h+" NOWRAP>";
for(var i=0; i<n; i++){
if(g.NonMetric==1) tlabel=twodec(String((g.scale*-1*(i+1)+g.offset)/100));
else tlabel=(g.scale*-1*(i+1)+g.offset);
st+="<DIV ALIGN=RIGHT style='font-family:"+gaf+";font-size:"+gas+"px;color:"+gac+";'>"+tlabel+"</DIV>";
if(i<(n-1)) st+="</TD></TR><TR><TD VALIGN=BOTTOM HEIGHT="+h+" NOWRAP>";
}
st+="</TD></TR></TABLE>";
st+="</TD>\n";
st+="<TD VALIGN=TOP ALIGN=RIGHT style='"+tdstyle+"'>";
st+="<TABLE BORDER="+stab()+" CELLSPACING=0 CELLPADDING=0>";
for(var i=0; i<n; i++){
st+="<TR><TD style='"+tdstyle+"' WIDTH=6 HEIGHT="+(h-1);
st+=">"+im+"</TD></TR>";
if(i<n){
st+="<TR><TD style='"+tdstyle+"' WIDTH=6 HEIGHT=1>";
if(g.gGrid){
st+="<DIV style='width:6px;height:1px;overflow:right;background-color:"+g.backcol+";text-align:left;'>";
st+="<DIV style='position:absolute;width:"+parseInt((g.jcount*(g.hscale+1)*g.icount)+8);
st+="px;height:1px;overflow:hidden;background-color:"+g.gGrid+";'></DIV>";
st+="</DIV></TD></TR>";
} 
else st+="<DIV style='width:6px;height:1px;overflow:hidden;background-color:"+g.backcol+";'></DIV></TD></TR>";
}
}  
st+="</TABLE>";
if(n==0) st+=im+">\n";
st+="</TD>\n";
st+="<TD VALIGN=TOP width=2 align=left style='"+tdstyle+"'>";
st+="<DIV style='width:1px;height:"+g.naxisheight+"px;overflow:hidden;background-color:"+g.backcol+";'></DIV>";
st+="</TD>\n";
return st;
}
function writeSG(g, min, max){
var h;
var p=g.posMax/(g.posMax-g.negMax);
h=Math.ceil(g.height/(g.posMax-g.negMax)*g.scale);
g.paxisheight=Math.ceil((g.posMax/g.scale)*h);
g.naxisheight=Math.ceil(((-1*g.negMax)/g.scale)*h);
g.ptoadd=(g.paxisheight - Math.floor(g.paxisheight/(g.paxisheight+g.naxisheight)*g.height))/g.posMax;
if(g.negMax) g.ntoadd=(g.naxisheight - Math.floor(g.naxisheight/(g.paxisheight+g.naxisheight)*g.height))/(g.negMax * -1);
else g.ntoadd=(g.naxisheight - Math.floor(g.naxisheight/(g.paxisheight+g.naxisheight)*g.height));
var n=Math.floor(g.posMax/g.scale);
var st="";
if(h<(parseInt(gas)+4)){
st+="<TD ROWSPAN=2>"+im+"</TD><TD ROWSPAN=2>"+im+"</TD><TD>"+im+"</TD>\n";
g.warning=1;
return st;
}
st+="<TD VALIGN=BOTTOM ALIGN=RIGHT ROWSPAN=2>";
st+="<TABLE BORDER="+stab()+" CELLSPACING=0 CELLPADDING=0><TR><TD VALIGN=BOTTOM HEIGHT=15 NOWRAP>";
for(var i=0; i<=n; i++){
if(g.NonMetric==1) label=twodec(String(Math.round(g.scale*(n)-g.scale*i+g.offset)/100));
else label=Math.round((g.scale*(n)-g.scale*i+g.offset)*100)/100;
if(g.IsWind && wdcp==1){
switch(label){
case 0:label="N";break;
case 45:label="NE";break;
case 90:label="E";break;
case 135:label="SE";break;
case 180:label="S";break;
case 225:label="SW";break;
case 270:label="W";break;
case 315:label="NW";break;
case 360:label="N";break;
}
}
st+="<DIV ALIGN=RIGHT style='font-family:"+gaf+";font-size:"+gas+"px;color:"+gac+"'>"+label+"</DIV>";
if(i<n) st+="</TD></TR><TR><TD VALIGN=BOTTOM HEIGHT="+h+" NOWRAP>";
}
st+="</TD></TR></TABLE>";
st+="</TD>\n";
st+="<TD VALIGN=BOTTOM ROWSPAN=2 ALIGN=RIGHT style='"+tdstyle+"'>";
st+="<TABLE BORDER="+stab()+" CELLSPACING=0 CELLPADDING=0>";
for(var i=0; i<=n; i++){
st+="<TR><TD style='"+tdstyle+"' WIDTH=6 HEIGHT=1>";
if(g.gGrid && i<n){
st+="<DIV style='width:6px;height:1px;overflow:right;background-color:"+g.backcol+";text-align:left;'>";
st+="<DIV style='position:absolute;width:"+parseInt((g.jcount*(g.hscale+1)*g.icount)+8);
st+="px;height:1px;overflow:hidden;background-color:"+g.gGrid+";'></DIV>";
st+="</DIV>";
st+="</TD></TR>";
} 
else st+="<DIV style='width:6px;height:1px;overflow:hidden;background-color:"+g.backcol+";'></DIV></TD></TR>";
if(i<n) st+="<TR><TD style='"+tdstyle+"' WIDTH=6 HEIGHT="+(h-1)+">"+im+"</TD></TR>";
}  
st+="</TABLE>";
st+="</TD>\n";
st+="<TD VALIGN=BOTTOM width=2 align=left style='"+tdstyle+"' HEIGHT="+(g.paxisheight+15)+">";
st+="<DIV style='width:1px;height:"+(g.paxisheight)+"px;overflow:hidden;background-color:"+g.backcol+";'></DIV>";
st+="</TD>\n";
return st;
}
function twodec(a){
if(a.lastIndexOf(".")<0) a+=".00";
else if(a.lastIndexOf(".")>(a.length)-3){ a+="0"; }
else if(a.lastIndexOf(".")>(a.length)-2){ a+="0"; }
return a;
}
function adjustOG(g){
for(var i=0; i<g.icount; i++)
for(var j=0; j<g.jcount; j++)
g.rows[i][j]-=g.offset;
}
function buildG(d){
doc=d || document;
if(!this.rows) return;
if(this.rows.length==0){
doc.write("<TABLE><TR><TD><TT>[empty graph]</TT></TD></TR></TABLE>\n");
return;
}
setGs(this);
adjustOG(this);
if(this.xScale) setDAG(this);
rescaleG(this);
buildRG(this, doc);
}
