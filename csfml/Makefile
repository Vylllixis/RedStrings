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
DEPS	=	$(OBJS:.o=.d)

#flags
CFLAGS	=	-Wall -Wextra -Werror -O2
CPPFLAGS	=	-I./include -I../includes
DEPFLAGS	=	-MMD -MP
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

$(OBJ_DIR)/%.o:	src/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ_DIR)

fclean:	clean
	$(RM) $(NAME)

re:	fclean all

-include $(DEPS)

.PHONY: all verifier clean fclean re
