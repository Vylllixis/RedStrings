##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

# ─── Fichiers ────────────────────────────────────────────────────────

NAME		=	redstrings

LIB_DIR		=	lib
LIB		=	$(LIB_DIR)/libmy.a

OBJ_DIR		=	obj
CSFML_DIR	=	csfml
DOC_DIR		=	docs/html

SRCS		=	$(shell find src -name '*.c')
LIB_SRCS	=	$(wildcard $(LIB_DIR)/*.c)

#src/main.c -> obj/src/main.o
OBJS		=	$(SRCS:%.c=$(OBJ_DIR)/%.o)
LIB_OBJS	=	$(LIB_SRCS:%.c=$(OBJ_DIR)/%.o)

#un .d par .o, écrit par gcc : la liste des .h utilisés par le .c
DEPS		=	$(OBJS:.o=.d) $(LIB_OBJS:.o=.d)

# ─── Compilation ─────────────────────────────────────────────────────

CC		=	gcc
AR		=	ar rcs
RM		=	rm -rf

CFLAGS		=	-Wall -Wextra -Werror
CPPFLAGS	=	-I./includes
DEPFLAGS	=	-MMD -MP
LDFLAGS		=	-L./$(LIB_DIR)
LDLIBS		=	-lmy

# ─── Règles ──────────────────────────────────────────────────────────

all:	$(NAME)

$(NAME):	$(OBJS) $(LIB)
	$(CC) -o $(NAME) $(OBJS) $(LDFLAGS) $(LDLIBS)

#bibliothèque personnelle : toutes les fonctions de lib/ dans libmy.a
lib:	$(LIB)

$(LIB):	$(LIB_OBJS)
	$(AR) $(LIB) $(LIB_OBJS)

$(OBJ_DIR)/%.o:	%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

#version graphique (CSFML) -> ./redstrings_csfml
csfml:	$(LIB)
	@$(MAKE) --no-print-directory -C $(CSFML_DIR)

clean:
	$(RM) $(OBJ_DIR)
	@$(MAKE) --no-print-directory -C $(CSFML_DIR) clean

fclean:	clean
	$(RM) $(NAME) $(LIB)
	@$(MAKE) --no-print-directory -C $(CSFML_DIR) fclean

re:
	@$(MAKE) --no-print-directory fclean
	@$(MAKE) --no-print-directory all

#comme re, mais ne garde que les exécutables
build:	re
	$(RM) $(OBJ_DIR)

doc:
	doxygen Doxyfile
	@echo "Documentation : $(DOC_DIR)/index.html"

doc-clean:
	$(RM) $(DOC_DIR)

-include $(DEPS)

.PHONY: all lib csfml clean fclean re build doc doc-clean
