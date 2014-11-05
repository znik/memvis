TARGET = analyzer
CXX = g++
CXXFLAGS = -Wall -O3 -std=c++0x
CXXLIBS =

all:
	$(CXX) $(CXXFLAGS) analyzer.cpp -o $(TARGET) $(CXXLIBS)

clean:
	rm -rf $(TARGET)

