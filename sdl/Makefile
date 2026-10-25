##
## ESGI PROJECT, 2026
## PROJET FIL ROUGE C Avancé
##

# ─── Fichiers ────────────────────────────────────────────────────────

NAME		=	../redstrings_sdl

OBJ_DIR		=	obj

SRCS		=	$(shell find src -name '*.c')
OBJS		=	$(SRCS:src/%.c=$(OBJ_DIR)/%.o)
DEPS		=	$(OBJS:.o=.d)

# ─── Compilation ─────────────────────────────────────────────────────

CC		=	gcc
RM		=	rm -rf

CFLAGS		=	-std=c11 -Wall -Wextra -Werror -pedantic -Wshadow \
			-Wmissing-prototypes -O2
#sdl2-config donne les -I et -l de SDL2 ; -isystem car ses en-têtes ne passent pas -Werror
SDL_CFLAGS	=	$(shell sdl2-config --cflags 2>/dev/null | sed 's/-I/-isystem /g')
SDL_LIBS	=	$(shell sdl2-config --libs 2>/dev/null)
CPPFLAGS	=	$(SDL_CFLAGS)
DEPFLAGS	=	-MMD -MP
LDLIBS		=	$(SDL_LIBS)

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

all:	verifier $(NAME)
	$(LOG_OK) "./$(notdir $(NAME)) est prêt"

verifier:
	@command -v sdl2-config >/dev/null 2>&1 || { printf "  $(C_ROUGE)✗$(C_RESET)      %s\n" \
		"SDL2 introuvable, installe-la avec : sudo apt install libsdl2-dev"; exit 1; }

$(NAME):	$(OBJS)
	$(LOG) "LD" "$(notdir $(NAME))"
	$(Q)$(CC) -o $(NAME) $(OBJS) $(LDLIBS)

$(OBJ_DIR)/%.o:	src/%.c
	$(LOG) "CC" "sdl/$<"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	$(LOG_RM) "RM" "sdl/$(OBJ_DIR)/"
	$(Q)$(RM) $(OBJ_DIR)

fclean:	clean
	$(LOG_RM) "RM" "redstrings_sdl"
	$(Q)$(RM) $(NAME)

re:
	$(Q)$(MAKE) fclean
	$(Q)$(MAKE) all

-include $(DEPS)

.PHONY: all verifier clean fclean re
