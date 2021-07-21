import { Graphics } from "./gfx.so";
var gfx = new Graphics(1024, 768, "test");
var quit = true;
function onkey(e)
{
    //console.log(e.timestamp, "- keyevent - isPressed", e.state, "- key", e.key);
    if(e.state && e.keycode == 101)
    {
        gfx.fillRect(gfx.rgb(255,255,255));
    }
}
function onquit(e){ 
    quit = false;
    //console.log(e.timestamp, "- quit");
}
function onmousemove(e){
    if(e.state & 1 == 1)
    {
        gfx.fillRect(e.x, e.y, 10, 10, gfx.rgb(128,0,128));
    }
}
gfx.on("keydown", onkey);
gfx.on("keyup", onkey);
gfx.on("quit", onquit);
gfx.on("mousemove", onmousemove);
gfx.initialize();
console.log(gfx.width, gfx.height, gfx.title);
var oldd = gfx.ticks;
var newd;
gfx.fillRect(gfx.rgb(255,255,255));
while(quit)
{
    gfx.pollEvent();
    gfx.update();
    newd = gfx.ticks;
    var tme = Math.round(1000/(newd - oldd));
    gfx.title = "test - fps: " + tme;
    oldd = newd;
    gfx.delay(1000/60);
}