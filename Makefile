CXX      := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98 -pedantic
NAME     := ircserv

INCDIR   := includes
SRCDIR   := src

SRC := $(SRCDIR)/main.cpp \
       $(SRCDIR)/Server.cpp \
       $(SRCDIR)/Client.cpp \
       $(SRCDIR)/Channel.cpp \
       $(SRCDIR)/CommandHandler.cpp \
       $(SRCDIR)/Utils.cpp \
	   $(SRCDIR)/Bot.cpp \
       $(SRCDIR)/FileTransfer.cpp

OBJ := $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	@$(CXX) $(CXXFLAGS) -I$(INCDIR) $(OBJ) -o $(NAME)

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	@$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	@rm -f $(OBJ)

fclean: clean
	@rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
