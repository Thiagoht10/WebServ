CXX = c++

FLAGS = -Wall -Werror -Wextra

INCLUDE = $(addprefix -I,$(shell find includes -type d))

NAME = webserv

SRC = main.cpp src/handler/config/ConfigLexer.cpp src/handler/ServerConfig.cpp \
src/handler/config/TokenStream.cpp

OBJ = $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(FLAGS) $(OBJ) -o $(NAME)

%.o: %.cpp
	$(CXX) $(FLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -rf $(OBJ)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re
