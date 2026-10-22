##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

# ─── Fichiers ────────────────────────────────────────────────────────

NAME		=	redstrings
NAME_DEBUG	=	redstrings_debug
NAME_TESTS	=	redstrings_tests

LIB_DIR		=	lib
LIB		=	$(LIB_DIR)/libmy.a

OBJ_DIR		=	obj
CSFML_DIR	=	csfml

SRCS		=	$(shell find src -name '*.c')
LIB_SRCS	=	$(wildcard $(LIB_DIR)/*.c)
TEST_SRCS	=	$(wildcard tests/*.c)

#src/main.c -> obj/src/main.o
OBJS		=	$(SRCS:%.c=$(OBJ_DIR)/%.o)
LIB_OBJS	=	$(LIB_SRCS:%.c=$(OBJ_DIR)/%.o)

#un .d par .o, écrit par gcc : la liste des .h utilisés par le .c
DEPS		=	$(OBJS:.o=.d) $(LIB_OBJS:.o=.d)

# ─── Compilation ─────────────────────────────────────────────────────

CC		=	gcc
AR		=	ar rcs
RM		=	rm -rf

CFLAGS		=	-std=c11 -Wall -Wextra -Werror -pedantic -Wshadow \
			-Wmissing-prototypes
CPPFLAGS	=	-I./includes
DEPFLAGS	=	-MMD -MP # regarde les modifications des .h
LDFLAGS		=	-L./$(LIB_DIR)
LDLIBS		=	-lmy

#make debug : symboles pour gdb + détection des erreurs mémoire
DEBUG_FLAGS	=	-g3 -O0 -fsanitize=address,undefined

# ─── Affichage ───────────────────────────────────────────────────────

#make V=1 affiche aussi les commandes complètes
#V = verbose "bavard" / D = Quiet "silence"
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

#tout est recompilé avec DEBUG_FLAGS, sans toucher à ./redstrings
debug:
	$(LOG) "DEBUG" "$(NAME_DEBUG)  ($(DEBUG_FLAGS))"
	$(Q)$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) $(SRCS) $(LIB_SRCS) \
		-o $(NAME_DEBUG)
	$(LOG_OK) "./$(NAME_DEBUG)  ou  gdb ./$(NAME_DEBUG)"

#make tests : compile tests/*.c avec le code du jeu (sans main.c) et les lance
tests:
	$(LOG) "TESTS" "$(NAME_TESTS)  ($(DEBUG_FLAGS))"
	$(Q)$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) $(TEST_SRCS) \
		$(filter-out src/main.c,$(SRCS)) $(LIB_SRCS) -o $(NAME_TESTS)
	$(Q)./$(NAME_TESTS)

#version graphique (CSFML) -> ./redstrings_csfml
csfml:	$(LIB)
	$(Q)$(MAKE) -C $(CSFML_DIR)

#version graphique avec sanitizers -> ./redstrings_csfml_debug
csfml-debug:	$(LIB)
	$(Q)$(MAKE) -C $(CSFML_DIR) debug

#tests de la version graphique (sans fenêtre)
csfml-tests:	$(LIB)
	$(Q)$(MAKE) -C $(CSFML_DIR) tests

clean:
	$(LOG_RM) "RM" "$(OBJ_DIR)/"
	$(Q)$(RM) $(OBJ_DIR)
	$(Q)$(MAKE) -C $(CSFML_DIR) clean

fclean:	clean
	$(LOG_RM) "RM" "$(NAME) $(NAME_DEBUG) $(NAME_TESTS) $(LIB)"
	$(Q)$(RM) $(NAME) $(NAME_DEBUG) $(NAME_TESTS) $(LIB)
	$(Q)$(MAKE) -C $(CSFML_DIR) fclean

re:
	$(Q)$(MAKE) fclean
	$(Q)$(MAKE) all

#comme re, mais ne garde que les exécutables
build:	re
	$(LOG_RM) "RM" "$(OBJ_DIR)/"
	$(Q)$(RM) $(OBJ_DIR)

help:
	@printf "  $(C_BLEU)make$(C_RESET)            compile ./$(NAME)\n"
	@printf "  $(C_BLEU)make lib$(C_RESET)        compile seulement $(LIB)\n"
	@printf "  $(C_BLEU)make debug$(C_RESET)      compile ./$(NAME_DEBUG) (gdb + sanitizers)\n"
	@printf "  $(C_BLEU)make tests$(C_RESET)      lance les tests unitaires (assert)\n"
	@printf "  $(C_BLEU)make csfml$(C_RESET)      compile la version graphique\n"
	@printf "  $(C_BLEU)make csfml-debug$(C_RESET) compile la version graphique avec les sanitizers\n"
	@printf "  $(C_BLEU)make csfml-tests$(C_RESET) lance les tests de la version graphique\n"
	@printf "  $(C_BLEU)make clean$(C_RESET)      supprime les .o\n"
	@printf "  $(C_BLEU)make fclean$(C_RESET)     supprime aussi les exécutables et $(LIB)\n"
	@printf "  $(C_BLEU)make re$(C_RESET)         fclean puis make\n"
	@printf "  $(C_BLEU)make V=1$(C_RESET)        affiche les commandes complètes\n"

-include $(DEPS)

.PHONY: all lib debug tests csfml csfml-debug csfml-tests clean fclean re build help
