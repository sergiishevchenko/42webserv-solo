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

SRCS		:= $(SRC_DIR)/main.cpp
OBJS		:= $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

INCLUDES	:= -I$(INC_DIR)

.PHONY: all clean fclean re

all: $(NAME)

$(NAME): $(OBJ_DIR) $(OBJS)
	$(CXX) $(CXXFLAGS) $(DEFS) $(OBJS) -o $(NAME)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(DEFS) $(INCLUDES) -c $< -o $@

clean:
	$(RM) -r $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: linux macos
linux:
	$(MAKE) DEFS="$(DEFS) -DWEBSERV_PLATFORM_LINUX"

macos:
	$(MAKE) DEFS="$(DEFS) -DWEBSERV_PLATFORM_DARWIN"


