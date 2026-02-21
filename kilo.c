/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editor_key{

	ARROW_LEFT = 1000, //after explicitly defining the 1st number, all the others incremement automatically.
	ARROW_RIGHT, //so arrow right is 1001, arrow up 1002, and so on.
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};
/*** data ***/

typedef struct e_row{

	int size;
	char *chars;
}e_row;

struct editor_config{

	int cx, cy;
	int screen_rows;
	int screen_cols;
	int num_rows;
	e_row row;
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

int editor_read_key(){
	
	int nread;
	char c;
	
	//stops the program from reading the input buffer so long as nothing changed. important for reading character by character in 'real' time.
	while((nread = read(STDIN_FILENO, &c, 1)) != 1){		
		if(nread == -1 && errno != EAGAIN) {die("read");}
	}
	
	if(c == '\x1b'){ //processing escape keys as input, such as for the arrow keys.
		
		char seq[3];
		
		if(read(STDIN_FILENO, &seq[0], 1) != 1) {return '\x1b';} //these are both to make sure the user didn't just press the escape key before attempting to process futher.
		if(read(STDIN_FILENO, &seq[1], 1) != 1) {return '\x1b';}
		
		if(seq[0] == '['){
			
			if(seq[1] >= '0' && seq[1] <= '9'){ // checking that there is an escape seq with an option attached to it, like 25H or whatever. but we wait to do something down**-->
			
				if(read(STDIN_FILENO, &seq[2], 1) != 1) {return '\x1b';}
				if(seq[2] == '~'){ //have to look at what the symbol after the number is first. this seems backwards compared to putting the symbol and then the number.
				
					switch (seq[1]){ //**--> here
					
						case '1':
						case '7': return HOME_KEY;
						case '4':
						case '8': return END_KEY; //i feel clever for using the fall through on switch statements to make things less cluttered (:
						
						case '3': return DEL_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
					}
				}
			}
			
			else{
				
				switch(seq[1]){
				
					case 'A': return ARROW_UP; //the arrow keys are read by terminal as <esc>[A, or B, etc. so that's what this is checking.
					case 'B': return ARROW_DOWN; //we convert the escape sequence into a single value that our process keypress function can make sense of.
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY; //why are there so many different potential ways for the home and end keys to be sent through...
				}
			}
		}
		
		else if(seq[0] == 'O'){ //are you kidding me. this is literally the only escape sequences that don't have the bracket up front. who did this. who even uses these keys.
		
			switch(seq[1]){
				
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}
	
		return '\x1b'; //for if we don't have anything bound to the escape sequence provided.
	}
	
	else {return c;} //all this logic above just for the cursor keys. everything else falls through to here.
	
}

int get_cursor_position(int *rows, int *cols){

	char buf[32];
	unsigned int i = 0;
	
	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {return -1;} //n is the "device status report" command, 6 means "get cursor position."
	
	while(i < sizeof(buf) - 1){ //this is a while loop because of common C parlance: only use for loops when you don't intend on breaking out for any reason.
		
		if(read(STDIN_FILENO, &buf[i], 1) != 1) {break;} //program goes out to get terminal program to print something, then goes back to read what it said.
		if(buf[i] == 'R') {break;}
		i++;		
	}
	
	buf[i] = '\0';
	
	if(buf[0] != '\x1b' || buf[1] != '[') {return -1;} //we skip this data but just want to make sure it's there.
	if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) {return -1;} //device status report formats the data with the semicolon in between.
	//the R at the end is still there, we just have no need to access it.
	
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


/*** file i/o ***/

void editor_open(char *filename){

	FILE *fp = fopen(filename, "r");
	if(!fp) {die("fopen");}
	
	char *line = NULL;
	size_t line_cap = 0; //what does it do :(
	ssize_t line_len;
	line_len = getline(&line, &line_cap, fp); //store the value of the line at the read cursor into the address of line. the function also conveniently returns the size of the line.
	if(line_len != -1){ //-1 = EOF
	
		while(line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {line_len--;} //line_len doesn't include newline or car return.
	
		E.row.size = line_len; //update the global state of the current line and its size.
		E.row.chars = malloc(line_len + 1);
		memcpy(E.row.chars, line, line_len);
		E.row.chars[line_len] = '\0';
		E.num_rows = 1;
	}
}


/*** append buffer ***/

struct abuf{
	
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void ab_append(struct abuf *ab, const char *s, int len){

	char *new = realloc(ab->b, ab->len + len); //simple dynamic memory reallocation. However here we reallocate to a new literal, not changing the original.
	
	if(new == NULL) {return;} //addressing NULL pointer is undefined behavior
	memcpy(&new[ab->len], s, len); //memcpy still needs to know how much extra data it's writing into, that's what len is doing there.
	ab->b = new;
	ab->len += len;
}

void ab_free(struct abuf *ab) {free(ab->b);}

/*** output ***/

void editor_draw_rows(struct abuf *ab){
	
	int y;
	for(y = 0; y < E.screen_rows; y++){
		
		if(y >= E.num_rows){ //all the blank lines are printed the same way.
		
			if(E.num_rows == 0 && y == E.screen_rows / 3){ //picks the exact top 3rd of the screen to print this special message. only if it's a blank file tho.
				
				char welcome[80]; //snprintf puts a string literal inside a buffer. buffer name, max bytes allowed into buffer, then the string literal, + extra formatted info
				int welcome_len = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
				
				if(welcome_len > E.screen_cols) {welcome_len = E.screen_cols;} //make sure we cut off any text going past the end of the window.
				
					int padding = (E.screen_cols - welcome_len)/2; //start in the middle of the screen, then subtract half the string's length, so the string is still centered
					if(padding){ //while padding is nonzero (always should be for any window that isn't literally 1 column tall.)
				
						ab_append(ab, "~", 1);
						padding--; //adjusting the padding to account for the tilde at the start of the row.
					}
				
				while(padding--) {ab_append(ab, " ", 1);} //i really don't know why this isn't a for loop, for(padding, padding > 0, padding--) would be clearer imo.
				ab_append(ab, welcome, welcome_len);
			}
			
			else{ab_append(ab, "~", 1);}
		}
		
		else{ //for printing out everything that has some stored state (say in a file that the program has open). AKA non-blank lines.
			
			int len = E.row.size;
			if(len > E.screen_cols) {len = E.screen_cols;}
			ab_append(ab, E.row.chars, len); //idk why we have to put len into its own value but E.row.chars is fine.
		}
		
		ab_append(ab, "\x1b[K", 3);
		
		if(y < E.screen_rows - 1) {ab_append(ab, "\r\n", 2);}
	}
}

void editor_refresh_screen(){
	
	struct abuf ab = ABUF_INIT; //the buffer refreshes back to null after every pass. stuff already sent to stdout still sticks around on the screen.
	
	ab_append(&ab, "\x1b[?25l", 6); //?25l hides the cursor, prevents flickering.
	ab_append(&ab, "\x1b[H", 3); //H code places cursor where you say. puts at the top left by default.
	
	editor_draw_rows(&ab);
	
	char buf[32]; //for cursor info
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1); //we add 1 to the coordinates because terminals use indexing starting at 1.
	ab_append(&ab, buf, strlen(buf));
	ab_append(&ab, "\x1b[?25h", 6); 
	
	write(STDOUT_FILENO, ab.b, ab.len); //prints the fully compiled screen buffer. again, this happens every 1/10th of a second.
	ab_free(&ab); //then clears it right after.
}


/*** input ***/

void editor_move_cursor(int key){

	switch(key){
		
		case ARROW_LEFT: if(E.cx != 0) {E.cx--;} break;
		case ARROW_RIGHT: if(E.cx != E.screen_cols - 1) {E.cx++;} break;
		case ARROW_UP: if(E.cy != 0) {E.cy--;} break;
		case ARROW_DOWN: if(E.cy != E.screen_rows - 1) {E.cy++;} break;
	}
}

void editor_process_keypress(){
	
	int c = editor_read_key();
	
	switch(c){
		case CTRL_KEY('q'): 
			
			write(STDOUT_FILENO, "\x1b[2J", 4);	//J clears the screen. 2J clears but leaves history. 3J wipes all terminal history.
			write(STDOUT_FILENO, "\x1b[3J", 4); //clears up the clutter left behind once exiting. i implemented this myself (:	
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0); 
			break;
		
		case HOME_KEY: E.cx -= (E.screen_cols/4); break; //make the cursor go in a square by doing end, pgup, home, pgdown. it's a fun little criss cross keyboard dance.
		case END_KEY: E.cx += (E.screen_cols/4); break;
		
		case PAGE_UP:
		case PAGE_DOWN: {// we have to make a code block inside the case so that we can declare a variable. c doesn't want you to rawdog it.
		
			//the tutorial wants the cursor all the way to the top or bottom. i think this is more fun.
			int times = E.screen_rows/4; //this is funny to me. we're looping the arrow up/down command as many times as there are rows. no extra processing needed in move_cursor!
			while(times--) {editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);}
		}break;
		
		case ARROW_UP:
		case ARROW_LEFT:
		case ARROW_DOWN:
		case ARROW_RIGHT: editor_move_cursor(c); break;
	}
}

/*** init ***/

void init_editor(){
	
	E.cx = 0;
	E.cy = 0;
	E.num_rows = 0;
	
	if(get_window_size(&E.screen_rows, &E.screen_cols) == -1) {die("get_window_size");}
}

int main(int argc, char *argv[]){
    
    enable_raw_mode();
    init_editor();
    if(argc >= 2) {editor_open(argv[1]);} //if an inline argument (the name of the file) was put in when calling the program, then open it with editor_open.
    
    while(1){
    	editor_refresh_screen();
       	editor_process_keypress();
  	}
    
    return 0;
}
