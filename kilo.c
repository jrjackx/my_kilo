/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/

struct termios orig_termios;

void die(const char *s){
	perror(s);
	exit(1);
}

/*** terminal ***/

void disable_raw_mode(void){
	
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {die("tcsetattr");}
}

void enable_raw_mode(void){

	if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) {die("tcgetattr");}
	atexit(disable_raw_mode);
	
	struct termios raw = orig_termios;	
	raw.c_iflag &= ~(BRKINT | IXON | ICRNL | INPCK | ISTRIP); //&= to turn the bits off.
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8); //|= to turn the bits on. notice i'm not not-ing this mask.
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {die("tcsetattr");}
}

char editor_read_key(){
	
	int nread;
	char c;
	while((nread = read(STDIN_FILENO, &c, 1)) != 1){		
		if(nread == -1 && errno != EAGAIN) {die("read");}
	}
	
	return c;
}


/*** input ***/

void editor_process_keypress(){
	
	char c = editor_read_key();
	
	switch(c){
		case CTRL_KEY('q'): exit(0); break;
	}
}

/*** init ***/

int main(){
    
    enable_raw_mode();
    
    while(1){
       	editor_process_keypress();
  	}
    
    return 0;
}
