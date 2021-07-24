import { Graphics } from "Graphics";
var gfx = new Graphics(1024, 768, "test");
var gfx2 = new Graphics(256, 256, "colours");
var quit = true;
function onkey(e)
{
    //console.log(e.timestamp, "- keyevent - isPressed", e.state, "- key", e.key);
    if(e.state && e.keycode == 101 && e.windowID == this.windowID)
    {
        this.fillRect(this.rgb(255,255,255));
    }
}
function onquit(e){ 
    quit = false;
    //console.log(e.timestamp, "- quit");
}
function onmousemove(e){
    if(e.state & 1 == 1 && e.windowID == this.windowID)
    {
        this.fillRect(e.x, e.y, 10, 10, this.rgb(128,0,128));
    }
}
gfx.on("keydown", onkey);
gfx2.on("keydown", onkey);
gfx.on("quit", onquit);
gfx2.on("quit", onquit);
gfx.on("close", onquit);
gfx2.on("close", onquit);
gfx.on("mousemove", onmousemove);
gfx2.on("mousemove", onmousemove);
gfx.initialize();
gfx2.initialize();
console.log(gfx.width, gfx.height, gfx.title);
console.log(gfx2.width, gfx2.height, gfx2.title);
var oldd = gfx.ticks;
var newd;
gfx.fillRect(gfx.rgb(255,255,255));
var pixels = new Uint32Array(gfx2.pixels);
while(quit)
{
    gfx.pollEvent();
    for(var x=0;x<256;x++) for(var y=0;y<256;y++)
        pixels[y*gfx2.width+x] = gfx2.rgb(x,y,0);
    gfx.update();
    gfx2.update();
    newd = gfx.ticks;
    var tme = Math.round(1000/(newd - oldd));
    gfx.title = "test - fps: " + tme;
    oldd = newd;
    gfx.delay(1000/60);
}