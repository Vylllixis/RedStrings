##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

#commande
CC	=	gcc
RM	=	rm -rf

NAME	=	../redstrings_csfml

LIB_DIR	=	../lib
LIB	=	$(LIB_DIR)/libmy.a

OBJ_DIR	=	obj

SRCS	=	$(wildcard src/*.c)

OBJS	=	$(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRCS))

#flags
CFLAGS	=	-W -Wall -Werror -O2
CPPFLAGS	=	-I./include -I../includes
LDFLAGS	=	-L$(LIB_DIR)
LDLIBS	=	-lmy -lcsfml-graphics -lcsfml-window -lcsfml-system -lm

all:	verifier
	@$(MAKE) --no-print-directory $(NAME)

verifier:
	@echo '#include <SFML/Graphics.h>' | $(CC) $(CPPFLAGS) -E -x c - > /dev/null 2>&1 \
	|| (echo "CSFML est introuvable, installe-la avec : sudo apt install libcsfml-dev" \
	&& exit 1)

$(NAME):	$(OBJS) $(LIB)
	$(CC) -o $(NAME) $(OBJS) $(LDFLAGS) $(LDLIBS)

$(OBJ_DIR)/%.o:	src/%.c include/gui.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ_DIR)

fclean:	clean
	$(RM) $(NAME)

re:	fclean all

.PHONY: all verifier clean fclean re
