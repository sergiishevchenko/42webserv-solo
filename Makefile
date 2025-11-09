NAME		:= webserv
CXX			:= c++

UNAME_S	:= $(shell uname -s)

CXXFLAGS	:= -Wall -Wextra -Werror -std=c++98
DEFS		:= -DWEBSERV_USE_POLL

ifeq ($(UNAME_S),Darwin)
DEFS		+= -DWEBSERV_PLATFORM_DARWIN
endif
ifeq ($(UNAME_S),Linux)
DEFS		+= -DWEBSERV_PLATFORM_LINUX
endif

SRC_DIR		:= src
OBJ_DIR		:= obj
INC_DIR		:= include

SRCS		:= $(SRC_DIR)/main.cpp \
			   $(SRC_DIR)/ConfigParser.cpp \
			   $(SRC_DIR)/Logger.cpp \
			   $(SRC_DIR)/Socket.cpp \
			   $(SRC_DIR)/Connection.cpp \
			   $(SRC_DIR)/Server.cpp
OBJS		:= $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

INCLUDES	:= -I$(INC_DIR)

# Colors
RESET		:= \033[0m
BOLD		:= \033[1m
RED			:= \033[1;31m
GREEN		:= \033[1;32m
YELLOW		:= \033[1;33m
BLUE		:= \033[1;34m
MAGENTA		:= \033[1;35m
CYAN		:= \033[1;36m
WHITE		:= \033[1;37m

# Symbols
CHECK		:= $(GREEN)✓$(RESET)
CROSS		:= $(RED)✗$(RESET)
ARROW		:= $(CYAN)→$(RESET)
GEAR		:= $(YELLOW)⚙$(RESET)
FOLDER		:= $(BLUE)📁$(RESET)
BIN		:= $(MAGENTA)🔧$(RESET)
TRASH		:= $(RED)🗑$(RESET)
ROCKET		:= $(GREEN)🚀$(RESET)

.PHONY: all clean fclean re header

all: header $(NAME)
	@echo ""
	@echo "$(GREEN)╔══════════════════════════════════════════╗$(RESET)"
	@echo "$(GREEN)║$(RESET)  $(GREEN)$(BOLD)Build completed successfully!$(RESET)           $(GREEN)║$(RESET)"
	@echo "$(GREEN)╚══════════════════════════════════════════╝$(RESET)"
	@echo "$(CYAN)$(BOLD)Binary: $(RESET)$(WHITE)$(NAME)$(RESET)"
	@echo ""

header:
	@echo "$(CYAN)$(BOLD)"
	@echo "╔═══════════════════════════════════════════════════════╗"
	@echo "║                                                       ║"
	@echo "║          $(MAGENTA)42webserv$(CYAN) - Building Project                 ║"
	@echo "║                                                       ║"
	@echo "╚═══════════════════════════════════════════════════════╝"
	@echo "$(RESET)"
	@echo "$(BLUE)$(BOLD)Platform:$(RESET) $(YELLOW)$(UNAME_S)$(RESET)"
	@echo "$(BLUE)$(BOLD)Compiler:$(RESET) $(YELLOW)$(CXX)$(RESET)"
	@echo "$(BLUE)$(BOLD)Flags:$(RESET) $(YELLOW)$(CXXFLAGS)$(RESET)"
	@echo "$(BLUE)$(BOLD)Source files:$(RESET) $(YELLOW)$(words $(SRCS))$(RESET)"
	@echo "$(BLUE)$(BOLD)Target:$(RESET) $(YELLOW)$(NAME)$(RESET)"
	@echo ""

$(NAME): $(OBJ_DIR) $(OBJS)
	@echo "$(YELLOW)┌──────────────────────────────────────────┐$(RESET)"
	@echo "$(YELLOW)│$(RESET)  $(YELLOW)$(BOLD)Linking object files...$(RESET)                 $(YELLOW)│$(RESET)"
	@echo "$(YELLOW)└──────────────────────────────────────────┘$(RESET)"
	@$(CXX) $(CXXFLAGS) $(DEFS) $(OBJS) -o $(NAME) && \
		echo "$(CHECK) $(GREEN)Linked $(RESET)$(CYAN)$(NAME)$(RESET)" || \
		(echo "$(CROSS) $(RED)Linking failed$(RESET)" && exit 1)

$(OBJ_DIR):
	@echo "$(BLUE)$(BOLD)Creating object directory...$(RESET)"
	@mkdir -p $(OBJ_DIR)
	@echo "$(CHECK) $(GREEN)Created $(RESET)$(FOLDER) $(CYAN)$(OBJ_DIR)$(RESET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "$(ARROW) $(CYAN)Compiling$(RESET) $(WHITE)$(notdir $<)$(RESET)$(CYAN)...$(RESET)"
	@$(CXX) $(CXXFLAGS) $(DEFS) $(INCLUDES) -c $< -o $@ && \
		echo "$(CHECK) $(GREEN)$(notdir $<) $(RESET)$(YELLOW)→$(RESET) $(CYAN)$(notdir $@)$(RESET)" || \
		(echo "$(CROSS) $(RED)Compilation failed: $(notdir $<)$(RESET)" && exit 1)

clean:
	@echo "$(RED)┌──────────────────────────────────────────┐$(RESET)"
	@echo "$(RED)│$(RESET)  $(RED)$(BOLD)Cleaning object files...$(RESET)                $(RED)│$(RESET)"
	@echo "$(RED)└──────────────────────────────────────────┘$(RESET)"
	@$(RM) -r $(OBJ_DIR) 2>/dev/null || true
	@echo "$(CHECK) $(GREEN)Removed $(RESET)$(CYAN)$(OBJ_DIR)$(RESET)"

fclean: clean
	@echo "$(RED)┌──────────────────────────────────────────┐$(RESET)"
	@echo "$(RED)│$(RESET)  $(RED)$(BOLD)Removing binary...$(RESET)                      $(RED)│$(RESET)"
	@echo "$(RED)└──────────────────────────────────────────┘$(RESET)"
	@$(RM) $(NAME) 2>/dev/null || true
	@echo "$(CHECK) $(GREEN)Removed $(RESET)$(CYAN)$(NAME)$(RESET)"

re: fclean all

.PHONY: linux macos format lint

linux:
	$(MAKE) DEFS="$(DEFS) -DWEBSERV_PLATFORM_LINUX"

macos:
	$(MAKE) DEFS="$(DEFS) -DWEBSERV_PLATFORM_DARWIN"

format:
	@echo "$(CYAN)┌──────────────────────────────────────────┐$(RESET)"
	@echo "$(CYAN)│$(RESET)  $(CYAN)$(BOLD)Formatting source files...$(RESET)            $(CYAN)│$(RESET)"
	@echo "$(CYAN)└──────────────────────────────────────────┘$(RESET)"
	@if command -v clang-format >/dev/null 2>&1; then \
		echo "$(ARROW) $(CYAN)Running clang-format...$(RESET)"; \
		find $(SRC_DIR) $(INC_DIR) -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i; \
		echo "$(CHECK) $(GREEN)Formatting complete$(RESET)"; \
	else \
		echo "$(CROSS) $(RED)Error: clang-format not found$(RESET)"; \
		echo "$(YELLOW)Install it with:$(RESET) $(CYAN)brew install clang-format$(RESET)"; \
		exit 1; \
	fi

CLANG_TIDY := $(shell command -v clang-tidy 2>/dev/null || echo "/usr/local/opt/llvm/bin/clang-tidy")

lint:
	@echo "$(CYAN)┌──────────────────────────────────────────┐$(RESET)"
	@echo "$(CYAN)│$(RESET)  $(CYAN)$(BOLD)Running static analysis...$(RESET)            $(CYAN)│$(RESET)"
	@echo "$(CYAN)└──────────────────────────────────────────┘$(RESET)"
	@if command -v $(CLANG_TIDY) >/dev/null 2>&1 || [ -f "$(CLANG_TIDY)" ]; then \
		echo "$(ARROW) $(CYAN)Running clang-tidy...$(RESET)"; \
		$(CLANG_TIDY) $(SRCS) \
			-header-filter='^$(INC_DIR)/.*' \
			-- $(CXXFLAGS) $(DEFS) $(INCLUDES) \
			2>&1 | grep -vE "(Suppressed.*warnings|non-user code|Use -header-filter|Use -system-headers|warnings generated|errors generated|Error while processing|too many errors|Processing file)" || true; \
		echo "$(CHECK) $(GREEN)Linting complete$(RESET)"; \
	else \
		echo "$(YELLOW)⚠$(RESET) $(YELLOW)Warning: clang-tidy not found$(RESET)"; \
		echo "$(YELLOW)Install it with:$(RESET) $(CYAN)brew install llvm$(RESET)"; \
		echo "$(YELLOW)Skipping linting...$(RESET)"; \
	fi
