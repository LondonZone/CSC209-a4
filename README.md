CSC209 Network Programming Assignment 

Unix sockets server for a text-based battle game, modelled after a Pokemon battle. 

 To connect to it, type the following at the shell: 
 
 stty -icanon; /bin/nc localhost 31300
 
 Gameplay:
 There are two kinds of attacks: 
 you can press 'a' for a regular attack or 'p' for a powermove. 
 
 - Regular attacks are weak but guaranteed to hit; 
 - Powermoves are strong, but not guaranteed to hit, 
   and limited in number. 
 - Also, you'll notice that a player is unaware of the quantity of powermoves held by their opponent. 
   So you never know whether your opponent is saving a powermove for later or they are all out of powermoves.

the other available option is to 's' speak something (in-game messaging) 
  - Only the currently-attacking player can talk, and saying something does not take up a turn.
  

<a rel="license" href="http://creativecommons.org/licenses/by-nc/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-nc/4.0/88x31.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-nc/4.0/">Creative Commons Attribution-NonCommercial 4.0 International License</a>.
