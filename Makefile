CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread

TARGET = inspecao_tuneis

# Adicionando a biblioteca do Mosquitto
LDFLAGS = -lpaho-mqtt3c

# Incluindo todos os arquivos .cpp
SRCS = src_cpp/main.cpp \
       src_cpp/NavigationManager.cpp \
       src_cpp/PIDController.cpp \
       src_cpp/tasks.cpp \
       src_cpp/Lidar.cpp \
       src_cpp/Odometria.cpp \
       src_cpp/Camera.cpp \
       src_cpp/Coletor.cpp \
       src_cpp/cpp_mqtt.cpp

all: clean $(TARGET)

$(TARGET):
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)