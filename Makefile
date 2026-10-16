##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

#commande
CC	=	gcc
RM	=	rm -rf

NAME	=	redstrings

DOXYGEN	=	doxygen
DOC_DIR	=	docs/html

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

doc:
	$(DOXYGEN) Doxyfile
	@echo "Documentation : $(DOC_DIR)/index.html"

doc-clean:
	$(RM) $(DOC_DIR)

.PHONY: all clean fclean re build doc doc-clean
