NAME = ft_ping

SOURCES = main.c\
			sources/ping.c

OBJECTS = $(SOURCES:.c=.o)

COMPILER = gcc

FLAGS = -Wall -Wextra -Werror -g3

LIBS = -lm

RM = rm -f

all:		$(NAME)

$(NAME):	$(OBJECTS)
			$(COMPILER) $(FLAGS) $(OBJECTS) $(LIBS) -o $(NAME)

%.o: %.c
			$(COMPILER) -c $(FLAGS) $< -o $@

clean:
			$(RM) $(OBJECTS)

fclean:		clean
			$(RM) $(NAME)

re:			fclean all

.PHONY: all clean fclean re