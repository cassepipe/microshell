CFLAGS			+= -fsanitize=address -g3 -fanalyzer -Wall -Wextra
LDFLAGS			+= -fsanitize=address -g3 -fanalyzer

NAME			= microshell

all:			${NAME}

${NAME}:		main.o	
				${CC} ${LDFLAGS} -o $@ $<

fclean			: 
				 rm -rf main.o $(NAME)

print_name:		
				@echo ${NAME}
