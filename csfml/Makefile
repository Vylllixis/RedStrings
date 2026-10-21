##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

# ─── Fichiers ────────────────────────────────────────────────────────

NAME		=	../redstrings_csfml

LIB_DIR		=	../lib
LIB		=	$(LIB_DIR)/libmy.a

OBJ_DIR		=	obj

SRCS		=	$(wildcard src/*.c)
OBJS		=	$(SRCS:src/%.c=$(OBJ_DIR)/%.o)

#moteur du jeu partagé avec la version console (aucun affichage dedans)
MOTEUR_DIR	=	../src/personnage
MOTEUR		=	personnage.c combat.c roster.c sauvegarde.c
OBJS_MOTEUR	=	$(MOTEUR:%.c=$(OBJ_DIR)/moteur/%.o)

DEPS		=	$(OBJS:.o=.d) $(OBJS_MOTEUR:.o=.d)

# ─── Compilation ─────────────────────────────────────────────────────

CC		=	gcc
RM		=	rm -rf

CFLAGS		=	-std=c11 -Wall -Wextra -Werror -pedantic -Wshadow \
			-Wmissing-prototypes -O2
#readlink() est POSIX : -std=c11 seul le masque
CPPFLAGS	=	-I./include -I../includes -D_POSIX_C_SOURCE=200809L
DEPFLAGS	=	-MMD -MP
LDFLAGS		=	-L$(LIB_DIR)
LDLIBS		=	-lmy -lcsfml-graphics -lcsfml-window -lcsfml-system -lm

# ─── Affichage (même style que le Makefile principal) ────────────────

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

LOG		=	@printf "  $(C_BLEU)%-6s$(C_RESET) %s\n"
LOG_OK		=	@printf "  $(C_VERT)✔$(C_RESET)      %s\n"
LOG_RM		=	@printf "  $(C_GRIS)%-6s %s$(C_RESET)\n"

# ─── Règles ──────────────────────────────────────────────────────────

all:	verifier lib $(NAME)
	$(LOG_OK) "./redstrings_csfml est prêt"

#libmy.a est construite par le Makefile principal
lib:
	$(Q)$(MAKE) -C .. lib

verifier:
	@echo '#include <SFML/Graphics.h>' | $(CC) $(CPPFLAGS) -E -x c - \
		> /dev/null 2>&1 || { printf "  $(C_ROUGE)✗$(C_RESET)      %s\n" \
		"CSFML introuvable, installe-la avec : sudo apt install libcsfml-dev"; \
		exit 1; }

$(NAME):	$(OBJS) $(OBJS_MOTEUR) $(LIB)
	$(LOG) "LD" "redstrings_csfml"
	$(Q)$(CC) -o $(NAME) $(OBJS) $(OBJS_MOTEUR) $(LDFLAGS) $(LDLIBS)

$(OBJ_DIR)/%.o:	src/%.c
	$(LOG) "CC" "csfml/$<"
	@mkdir -p $(OBJ_DIR)
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/moteur/%.o:	$(MOTEUR_DIR)/%.c
	$(LOG) "CC" "src/personnage/$(notdir $<)"
	@mkdir -p $(OBJ_DIR)/moteur
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	$(LOG_RM) "RM" "csfml/$(OBJ_DIR)/"
	$(Q)$(RM) $(OBJ_DIR)

fclean:	clean
	$(LOG_RM) "RM" "redstrings_csfml"
	$(Q)$(RM) $(NAME)

re:
	$(Q)$(MAKE) fclean
	$(Q)$(MAKE) all

-include $(DEPS)

.PHONY: all verifier lib clean fclean re
