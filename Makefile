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

CSFML_DIR	=	csfml

OBJS	=	$(SRCS:.c=.o)

#flags
CFLAGS	=	-W -Wall -Werror
CPPFLAGS	=	-I./includes

all:	$(NAME)

$(NAME):	$(OBJS)
	$(CC) -o $(NAME) $(OBJS)

#version graphique (CSFML) -> ./redstrings_csfml
csfml:
	@$(MAKE) --no-print-directory -C $(CSFML_DIR)

clean:
	$(RM) $(OBJS)
	@$(MAKE) --no-print-directory -C $(CSFML_DIR) clean

fclean:	clean
	$(RM) $(NAME)
	@$(MAKE) --no-print-directory -C $(CSFML_DIR) fclean

re: fclean all

build:	fclean all
	$(RM) $(OBJS)

doc:
	$(DOXYGEN) Doxyfile
	@echo "Documentation : $(DOC_DIR)/index.html"

doc-clean:
	$(RM) $(DOC_DIR)

.PHONY: all csfml clean fclean re build doc doc-clean
