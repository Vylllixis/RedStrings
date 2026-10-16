##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

#commande
CC	=	gcc
AR	=	ar rcs
RM	=	rm -rf

NAME	=	redstrings

LIB_DIR	=	lib
LIB	=	$(LIB_DIR)/libmy.a

DOXYGEN	=	doxygen
DOC_DIR	=	docs/html

SRCS	=	$(shell find ./src -name '*.c')
LIB_SRCS	=	$(wildcard $(LIB_DIR)/*.c)

CSFML_DIR	=	csfml

OBJS	=	$(SRCS:.c=.o)
LIB_OBJS	=	$(LIB_SRCS:.c=.o)

#flags
CFLAGS	=	-W -Wall -Werror
CPPFLAGS	=	-I./includes
LDFLAGS	=	-L./$(LIB_DIR)
LDLIBS	=	-lmy

all:	$(NAME)

$(NAME):	$(OBJS) $(LIB)
	$(CC) -o $(NAME) $(OBJS) $(LDFLAGS) $(LDLIBS)

#bibliothèque personnelle : toutes les fonctions de lib/ dans libmy.a
lib:	$(LIB)

$(LIB):	$(LIB_OBJS)
	$(AR) $(LIB) $(LIB_OBJS)

#version graphique (CSFML) -> ./redstrings_csfml
csfml:	$(LIB)
	@$(MAKE) --no-print-directory -C $(CSFML_DIR)

clean:
	$(RM) $(OBJS) $(LIB_OBJS)
	@$(MAKE) --no-print-directory -C $(CSFML_DIR) clean

fclean:	clean
	$(RM) $(NAME) $(LIB)
	@$(MAKE) --no-print-directory -C $(CSFML_DIR) fclean

re: fclean all

build:	fclean all
	$(RM) $(OBJS)

doc:
	$(DOXYGEN) Doxyfile
	@echo "Documentation : $(DOC_DIR)/index.html"

doc-clean:
	$(RM) $(DOC_DIR)

.PHONY: all lib csfml clean fclean re build doc doc-clean
