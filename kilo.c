/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/

struct editor_config{

	int screen_rows;
	int screen_cols;
	struct termios orig_termios;
};

struct editor_config E;

void die(const char *s){
	
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	
	perror(s);
	exit(1);
}

/*** terminal ***/

void disable_raw_mode(void){
	//all of these "die" conditionals are checked after running the function they're attached to, which does something else first.
	//for instance here, the terminal is set back to orig_termios settings. then it checks for an error afterwards.
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {die("tcsetattr");}
}

void enable_raw_mode(void){

	if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {die("tcgetattr");}
	atexit(disable_raw_mode);
	
	struct termios raw = E.orig_termios;	
	raw.c_iflag &= ~(BRKINT | IXON | ICRNL | INPCK | ISTRIP); //&= to turn the bits off.
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8); //|= to turn the bits on. notice i'm not not-ing this mask.
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1; //these two commands make the terminal constantly check for input, returning zero if nothing came in the last 1/10th of a second.
	
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {die("tcsetattr");}
}

char editor_read_key(){
	
	int nread;
	char c;
	
	//stops the program from reading the input buffer so long as nothing changed. important for reading character by character in 'real' time.
	while((nread = read(STDIN_FILENO, &c, 1)) != 1){		
		if(nread == -1 && errno != EAGAIN) {die("read");}
	}
	
	return c;
}

int get_cursor_position(int *rows, int *cols){

	char buf[32];
	unsigned int i = 0;
	
	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {return -1;} //n is the "device status report" command, 6 means "get cursor position."
	
	while(i < sizeof(buf) - 1){ //this is a while loop because of common C parlance: only use for loops when you don't intend on breaking out for any reason.
		
		if(read(STDIN_FILENO, &buf[i], 1) != 1) {break;} //program goes out to get terminal program to print something, then goes back to read what it said.
		if(buf[i] == 'R') {break;} //load it into the buffer.
		i++;		
	}
	
	buf[i] = '\0';
	
	if(buf[0] != '\x1b' || buf[1] != '[') {return -1;} //we skip this data but just want to make sure it's there.
	if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) {return -1;} //device status report formats the data with the semicolon in between
	
	return 0;
}

int get_window_size(int *rows, int *cols){

	struct winsize ws;
	
	// grabs the row and column values from an ioctl operation and puts it in a 'winsize' struct.
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
	
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {return -1;} //C moves the cursor right, B moves the cursor down, stopping if it hit the edge.
		return get_cursor_position(rows, cols); //only calls if ioctl fails, which it realistically never should. a lot of work for an edge case if you ask me.
	}
	
	else{
		
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0; //we pass these values into the ints inside our editor_config struct.
	}
}

/*** output ***/

void editor_draw_rows(){
	
	int y;
	for(y = 0; y < E.screen_rows; y++){
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}

void editor_refresh_screen(){
	
	write(STDOUT_FILENO, "\x1b[2J", 4); //J code clears the screen
	write(STDOUT_FILENO, "\x1b[H", 3); //H code places cursor back at the top of the screen.
	
	editor_draw_rows();
	write(STDOUT_FILENO, "\x1b[H", 3); 
}


/*** input ***/

void editor_process_keypress(){
	
	char c = editor_read_key();
	
	switch(c){
		case CTRL_KEY('q'): 
			
			write(STDOUT_FILENO, "\x1b[2J", 4);	
			write(STDOUT_FILENO, "\x1b[3J", 4); //clears up the clutter left behind once exiting. i implemented this myself (:	
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0); 
			break;
	}
}

/*** init ***/

void init_editor(){
	
	if(get_window_size(&E.screen_rows, &E.screen_cols) == -1) {die("get_window_size");}
}

int main(){
    
    enable_raw_mode();
    init_editor();
    
    while(1){
    	editor_refresh_screen();
       	editor_process_keypress();
  	}
    
    return 0;
}
