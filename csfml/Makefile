##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

# ─── Fichiers ────────────────────────────────────────────────────────

NAME		=	../redstrings_csfml
NAME_DEBUG	=	../redstrings_csfml_debug
NAME_TESTS	=	../redstrings_csfml_tests

LIB_DIR		=	../lib
LIB		=	$(LIB_DIR)/libmy.a

OBJ_DIR		=	obj
OBJ_DEBUG	=	obj_debug

#toutes les sources, quel que soit le sous-dossier de src/
SRCS		=	$(shell find src -name '*.c')
OBJS		=	$(SRCS:src/%.c=$(OBJ_DIR)/%.o)

#moteur du jeu partagé avec la version console (aucun affichage dedans)
MOTEUR_DIR	=	../src/personnage
MOTEUR		=	personnage.c combat.c bataille.c roster.c sauvegarde.c
OBJS_MOTEUR	=	$(MOTEUR:%.c=$(OBJ_DIR)/moteur/%.o)

#fonctions de la version graphique qui ne dépendent pas de SFML : testables sans fenêtre
TEST_SRCS	=	tests/tests.c src/outils/anim.c src/outils/jeu.c src/outils/utf8.c \
			src/ui/libelles.c $(MOTEUR_DIR)/personnage.c

DEPS		=	$(OBJS:.o=.d) $(OBJS_MOTEUR:.o=.d)

# ─── Compilation ─────────────────────────────────────────────────────

CC		=	gcc
RM		=	rm -rf

#avertissements en plus de ceux du projet principal : formats, const, pointeurs nuls, VLA, flottants
WSUPP		=	-Wformat=2 -Wcast-qual -Wnull-dereference -Wwrite-strings \
			-Wstrict-prototypes -Wvla -Wundef -Wlogical-op -Wfloat-equal
CFLAGS		=	-std=c11 -Wall -Wextra -Werror -pedantic -Wshadow \
			-Wmissing-prototypes $(WSUPP) -O2
#make debug : -O0, symboles et sanitizers (comme le Makefile principal)
SANITIZE	=	-fsanitize=address,undefined
CFLAGS_DEBUG	=	-std=c11 -Wall -Wextra -Werror -pedantic -Wshadow \
			-Wmissing-prototypes $(WSUPP) -g3 -O0 $(SANITIZE)
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
	$(LOG_OK) "./$(notdir $(NAME)) est prêt"

#libmy.a est construite par le Makefile principal
lib:
	$(Q)$(MAKE) -C .. lib

verifier:
	@echo '#include <SFML/Graphics.h>' | $(CC) $(CPPFLAGS) -E -x c - \
		> /dev/null 2>&1 || { printf "  $(C_ROUGE)✗$(C_RESET)      %s\n" \
		"CSFML introuvable, installe-la avec : sudo apt install libcsfml-dev"; \
		exit 1; }

$(NAME):	$(OBJS) $(OBJS_MOTEUR) $(LIB)
	$(LOG) "LD" "$(notdir $(NAME))"
	$(Q)$(CC) -o $(NAME) $(OBJS) $(OBJS_MOTEUR) $(LDFLAGS) $(LDLIBS)

$(OBJ_DIR)/%.o:	src/%.c
	$(LOG) "CC" "csfml/$<"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/moteur/%.o:	$(MOTEUR_DIR)/%.c
	$(LOG) "CC" "src/personnage/$(notdir $<)"
	@mkdir -p $(OBJ_DIR)/moteur
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

#version de débogage : une autre arborescence d'objets, pour ne rien mélanger
debug:	verifier lib
	$(Q)$(MAKE) NAME=$(NAME_DEBUG) OBJ_DIR=$(OBJ_DEBUG) CFLAGS="$(CFLAGS_DEBUG)" \
		LDFLAGS="$(LDFLAGS) $(SANITIZE)" $(NAME_DEBUG)
	$(LOG_OK) "./$(notdir $(NAME_DEBUG)) est prêt"

#tests unitaires sans fenêtre, avec les sanitizers
tests:	lib
	$(LOG) "TESTS" "$(notdir $(NAME_TESTS))  ($(SANITIZE))"
	$(Q)$(CC) $(CFLAGS_DEBUG) $(CPPFLAGS) $(TEST_SRCS) $(LDFLAGS) -lmy -lm -o $(NAME_TESTS)
	$(Q)./$(NAME_TESTS)
	$(LOG_OK) "Tous les tests de la version graphique passent"

clean:
	$(LOG_RM) "RM" "csfml/$(OBJ_DIR)/ csfml/$(OBJ_DEBUG)/"
	$(Q)$(RM) $(OBJ_DIR) $(OBJ_DEBUG)

fclean:	clean
	$(LOG_RM) "RM" "redstrings_csfml redstrings_csfml_debug redstrings_csfml_tests"
	$(Q)$(RM) $(NAME) $(NAME_DEBUG) $(NAME_TESTS)

re:
	$(Q)$(MAKE) fclean
	$(Q)$(MAKE) all

-include $(DEPS)

.PHONY: all verifier lib debug tests clean fclean re
