##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

#commande
CC	=	gcc
RM	=	rm -rf

NAME	=	redstrings

SRCS	=	$(shell find ./src -name '*.c')

OBJS	=	$(SRCS:.c=.o)

#flags
CFLAGS	=	-W -Wall -Werror
CPPFLAGS	=	-I./includes

all:	$(NAME)

$(NAME):	$(OBJS)
	$(CC) -o $(NAME) $(OBJS)

clean:
	$(RM) $(OBJS)

fclean:	clean
	$(RM) $(NAME)

re: fclean all

build:	fclean all
	$(RM) $(OBJS)