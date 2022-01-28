.PHONY: all libs main clean

all: libs main

libs:
	g++ -c Source/Tools.cpp -o Build/Tools.o -I Source/ -lGLEW -lGLU -lGL -lSDL2
	g++ -c Source/OBJLoader.cpp -o Build/OBJLoader.o -I Source/ -L Build/ -I '3rd party/includes/'
	g++ -c Source/GeometricMesh.cpp -o Build/GeometricMesh.o -I Source/ -L Build/ -I '3rd party/includes/'
	g++ -c Source/TextureManager.cpp -o Build/TextureManager.o -I Source/ -L Build/ -lGLEW -lGLU -lGL -lSDL2 -lSDL2_image -I '3rd party/includes/'
	g++ -c Source/ShaderProgram.cpp -o Build/ShaderProgram.o -I Source/ -L Build/ -lGLEW -lGLU -lGL -lSDL2 -I '3rd party/includes/'
	g++ -c Source/AssetManager.cpp -o Build/AssetManager.o -I Source/ -L Build/ -lGLEW -lGLU -lGL -lSDL2
	g++ -c Source/GeometryNode.cpp -o Build/GeometryNode.o -I Source/ -L Build/ -lGLEW -lGLU -lGL -I '3rd party/includes/'
	g++ -c Source/LightNode.cpp -o Build/LightNode.o -I Source/ -L Build/ -lGLEW -lGLU -lGL -I '3rd party/includes/'
	g++ -c Source/CollidableNode.cpp -o Build/CollidableNode.o -I Source/ -L Build/ -I '3rd party/includes/'
	g++ -c Source/Renderer.cpp -o Build/Renderer.o -I Source/ -L Build/ -lGLEW -lGLU -lGL -lSDL2 -lSDL2_image -I '3rd party/includes/'
	# g++ Source/*.cpp -o Build/customlib.o -I Source/ -lGLEW -lGLU -lGL -lSDL2 -lSDL2_image -I '3rd party/includes/'

main:
	g++ Source/main.cpp Build/*.o -o main -I Source/ -L Build/ -lGLEW -lGLU -lGL -lSDL2 -lSDL2_image -I '3rd party/includes/'

clean:
	rm -rf Build/*
	rm main
