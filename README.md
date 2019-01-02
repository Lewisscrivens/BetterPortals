# UE4BetterPortals
VR portals that are seemless. Using scenecapture2D and depth masks.

To start with I just used the simple portals template from unreal but if you do this you will see a few differences not big but noticable
and its aparently what allot of people struggle to get past.

First thing is motion blur in the global post process volume, if you turn this off then it will remove motion blur which increases the quality.

Next thing to do would be to check use viewport size RT, this will use the viewport/game resolution on the portal to match pixel dencity.

Next, you want to go into the class settings for the portal and change the tick group to postupdatework. This will make the updating of 
the render texture happen straight after the position and rotation code is ran.

Finally you should be left with a portal that looks quite nice as far as single pane portals go im pretty sure that all their is left that
isnt so seemless is when you move back and forth from the portal the render texture in the centre seems to swap in and out. This is 
due to the sampling mode in the project settings, just change this to FXAA, youd want to do this in VR anyway.

As for better portals than this all I can come up with at the moment is having an inside out shape of some sort with the texture rendered
on the inside so that the teleportation is very seemless, test the project to see the difference. Further work on this which I will do is
trying to occlude the other portal frame etc. when you havent teleported yet from opposing scenecapture2D's.

The engine version is 4.21.1 UE4. 
I only spent likean hour or so on this aswell so it is abit messy for now. I didnt change much BP either.
