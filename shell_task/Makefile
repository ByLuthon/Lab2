.PHONY : shell
shell : shell_sig shell_poll


shell_sig: shell.c
	gcc -Wall -o shell_sig -D SIGNAL_DETECTION=1 shell.c

shell_poll: shell.c
	gcc -Wall -o shell_poll -D SIGNAL_DETECTION=0 shell.c

clean:
	rm shell_sig shell_poll
