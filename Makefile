all: tools shaderprogram geometricmesh geometricnode objloader renderer main

tools:
	g++ -c Source/Tools.cpp -o Build/Tools.o -lGL -lGLEW -lglut

shaderprogram:
	g++ -c Source/ShaderProgram.cpp -o Build/ShaderProgram.o -lGL -lGLEW -lglut -lSDL2 -l Source/

geometricmesh:
	g++ -c Source/GeometricMesh.cpp -o Build/GeometricMesh.o -lGL -lGLEW -lglut -l Source/

geometricnode:
	g++ -c Source/GeometryNode.cpp -o Build/GeometryNode.o -lGL -lGLEW -lglut -l Source/

objloader:
	g++ -c Source/OBJLoader.cpp -o Build/OBJLoader.o -lGL -lGLEW -lglut -l Source/

renderer:
	g++ -c Source/Renderer.cpp -o Build/Renderer.o -lGL -lGLEW -lglut -lSDL2 -lSDL2_image -l Source/

main:
	g++ Build/*.o Source/main.cpp -o main -lGL -lGLEW -lglut -lSDL2

clean:
	rm Build/*
	rm main
