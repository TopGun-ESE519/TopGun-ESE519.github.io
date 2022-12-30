# TopGun-ESE519.github.io

## Project Details:

1. The game can be visualized as a grid of pixels. The jet and missiles are genereated from a set of pixels.

2. When the game begins, the jet appears whose initial position is the center of the left end. At the same time, seven missles are fired from the right end of the screen moving towards the left end.

3. The missiles only move forward along their horizontal axis and their vertical coords remains the same. Whereas, the jet the free to move in any direction with use of joystick placing the entire control in the hands of the user.

4. One loop accounts for one forward movement of the missile (the missile will jump some pixels on the grid depending upon it's speed). The loops will continue until the missile's face reaches the left end. After that the missile will be regenerated again at the right end and this algorithm works independently for each missile.

5. Every missile moves at a random speed which depends upon the random number generated at the start of each loop.

6. Dimensions of missile and jet are 30x17 pixels. The wings size differ for both, 10 pixels for jet and 5 pixels for missile. With one adc movement the jet moves 15 pixels according to the command. The adc instructions are given after every loop.

7. At the end of every loop the algorithm checks if the pixels of jet overlaps with the pixels of missiles i.e if there is a crash. If this happens then the jet is crashed and "Game Over" message is dispayed. If the user surfs successfully through the screen then they win the game. A computationally efficient logic is implemented for crash check, avoiding any lags, which will be expained in a latter section.

8. An audio is added for enhancing the user experience.

## Brief overview of code:

1. The c program starts from the main function which initializes and sets GPIO pins, state machines and interrupts. 

2. At the end of the main function, we call the game function which does the task of handling the entire game with the help of some external functions. 

3. The game function starts with clearing off  any “game over” or “you win” message. 

4. Then it initializes the starting x-positions of each missile. 

5. After that, each missile gets displayed at their respective locations for 1 second and gets cleared off from the screen.

6. It also takes ADC input from the joystick and moves the fighter jet accordingly. 

7. For collision detection we are simply applying filters to get in what missile’s range the fighter jet is falling using its y-coordinate and then we compare the x-coordinate of the fighter jet with that of the missile. 

8. If there is a collision then we call the game over function that displays the “game over” message, stops the game and waits for the player to hit the replay button. 

9. When the player successfully reaches the right end of the screen then we call  a game win function that displays a “you win” message, stops the game and waits for the player to hit the replay button. 

10. At the end we call getrandom function that generates a random number in order to move each missile at random speed.

##  Block diagram of the code:

![image](https://user-images.githubusercontent.com/73771085/210114495-d1a5993e-a175-4553-ab43-e39c3dc83681.png)
