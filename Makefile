#  * Name: Makefile
#  * Author: Harris Hamid
#  * Pledge : I pledge my honor that I have abided by the Stevens Honor System.
 
all:
	sudo apt-get install figlet
	gcc server.c -o server
	gcc client.c -o client