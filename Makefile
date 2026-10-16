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

# ─── Affichage ───────────────────────────────────────────────────────

#make V=1 affiche aussi les commandes complètes
ifeq ($(V), 1)
    Q	=
else
    Q	=	@
endif

MAKEFLAGS	+=	--no-print-directory

C_BLEU		=	\033[1;34m
C_VERT		=	\033[1;32m
C_ROUGE		=	\033[1;31m
C_GRIS		=	\033[90m
C_RESET		=	\033[0m

#utilisation : $(LOG) ÉTIQUETTE "texte"
LOG		=	@printf "  $(C_BLEU)%-6s$(C_RESET) %s\n"
LOG_OK		=	@printf "  $(C_VERT)✔$(C_RESET)      %s\n"
LOG_RM		=	@printf "  $(C_GRIS)%-6s %s$(C_RESET)\n"

# ─── Règles ──────────────────────────────────────────────────────────

all:	$(NAME)
	$(LOG_OK) "./$(NAME) est prêt"

$(NAME):	$(OBJS) $(LIB)
	$(LOG) "LD" "$@"
	$(Q)$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

#bibliothèque personnelle : toutes les fonctions de lib/ dans libmy.a
lib:	$(LIB)
	@:

$(LIB):	$(LIB_OBJS)
	$(LOG) "AR" "$@  ($(words $^) fonctions)"
	$(Q)$(RM) $@
	$(Q)$(AR) $@ $^

$(OBJ_DIR)/%.o:	%.c
	$(LOG) "CC" "$<"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

#version graphique (CSFML) -> ./redstrings_csfml
csfml:	$(LIB)
	$(Q)$(MAKE) -C $(CSFML_DIR)

clean:
	$(LOG_RM) "RM" "$(OBJ_DIR)/"
	$(Q)$(RM) $(OBJ_DIR)
	$(Q)$(MAKE) -C $(CSFML_DIR) clean

fclean:	clean
	$(LOG_RM) "RM" "$(NAME) $(LIB)"
	$(Q)$(RM) $(NAME) $(LIB)
	$(Q)$(MAKE) -C $(CSFML_DIR) fclean

re:
	$(Q)$(MAKE) fclean
	$(Q)$(MAKE) all

#comme re, mais ne garde que les exécutables
build:	re
	$(LOG_RM) "RM" "$(OBJ_DIR)/"
	$(Q)$(RM) $(OBJ_DIR)

doc:
	$(LOG) "DOC" "$(DOC_DIR)/index.html"
	$(Q)doxygen Doxyfile

doc-clean:
	$(LOG_RM) "RM" "$(DOC_DIR)"
	$(Q)$(RM) $(DOC_DIR)

-include $(DEPS)

.PHONY: all lib csfml clean fclean re build doc doc-clean
