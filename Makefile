CXX      := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98 -pedantic
NAME     := ircserv

INCDIR   := includes
SRCDIR   := src

SRC := main.cpp \
       Server.cpp \
       Client.cpp \
       Channel.cpp \
       CommandHandler.cpp \
       Utils.cpp \
       Bot.cpp \
       FileTransfer.cpp

OBJDIR := obj
OBJ := $(SRC:%.cpp=$(OBJDIR)/%.o)
SRC := $(SRC:%.cpp=$(SRCDIR)/%.cpp)

all: $(NAME)

$(NAME): $(OBJ)
	@$(CXX) $(CXXFLAGS) -I$(INCDIR) $(OBJ) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	@$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	@rm -f $(OBJ)
	@rm -rf $(OBJDIR)

fclean: clean
	@rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
