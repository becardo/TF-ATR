CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread

TARGET = inspecao_tuneis

# Incluindo todos os arquivos .cpp que o Pedro fez e os que vamos usar
SRCS = src_cpp/main.cpp \
       src_cpp/NavigationManager.cpp \
       src_cpp/PIDController.cpp \
       src_cpp/tasks.cpp \
       src_cpp/Lidar.cpp \
       src_cpp/Odometria.cpp \
       src_cpp/Camera.cpp \
       src_cpp/Coletor.cpp

all: clean $(TARGET)

$(TARGET):
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)