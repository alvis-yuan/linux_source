# Compiler and tools
CXX := g++
AR := ar
RM := rm -f
MKDIR := mkdir -p

# Project structure
SRC_DIRS := iAP2Link iAP2Utility iAP2UtilityImplementation
BUILD_DIR := build
LIB_DIR := lib
OBJ_DIR := $(BUILD_DIR)/obj

# Target library name
TARGET := $(LIB_DIR)/libiAP2.so

# Source files (automatically find all .cpp files in source directories)
CPP_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
# Source files can also include .c files if needed
C_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
SRC_FILES := $(CPP_FILES) $(C_FILES)

# Object files (replace .cpp/.c with .o and place in build directory)
OBJ_FILES := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CPP_FILES)) \
             $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_FILES))

# Include directories (assuming each source directory has its own headers)
INCLUDE_DIRS := $(SRC_DIRS) .
INCLUDE_FLAGS := $(addprefix -I,$(INCLUDE_DIRS))

# Compiler flags
CXXFLAGS := -Wall -Wextra -Werror -fPIC -std=c++11 $(INCLUDE_FLAGS) 
CFLAGS := -Wall -Wextra -Werror -fPIC $(INCLUDE_FLAGS) -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=type-limits -Wno-error=unused-function -Wno-error=pointer-to-int-cast

# Linker flags for shared library
LDFLAGS := -shared -fPIC
LDLIBS :=

# Debug/Release configuration
ifeq ($(DEBUG),1)
    CXXFLAGS += -g -O0 -DDEBUG
    CFLAGS += -g -O0 -DDEBUG
else
    CXXFLAGS += -O2 -DNDEBUG
    CFLAGS += -O2 -DNDEBUG
endif

# Default target
all: $(TARGET)

# Create the shared library from object files
$(TARGET): $(OBJ_FILES) | $(LIB_DIR)
	@echo "Linking $@"
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "Build complete: $@"

# Create necessary directories
$(OBJ_DIR) $(LIB_DIR):
	$(MKDIR) $@

# Compile C++ source files
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	@$(MKDIR) $(dir $@)
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile C source files (if any)
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@$(MKDIR) $(dir $@)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Generate compile_commands.json for tooling support
compile_commands.json: $(SRC_FILES)
	@echo "Generating compile_commands.json"
	@echo "[" > $@
	@for file in $(SRC_FILES); do \
		dir=$$(pwd); \
		obj=$$(echo $$file | sed 's/\.[^.]*$$/.o/'); \
		obj_path="$(OBJ_DIR)/$$obj"; \
		if [[ $$file == *.cpp ]]; then \
			cmd="$(CXX) $(CXXFLAGS) -c $$file -o $$obj_path"; \
		else \
			cmd="$(CC) $(CFLAGS) -c $$file -o $$obj_path"; \
		fi; \
		echo "  {" >> $@; \
		echo "    \"directory\": \"$$dir\"," >> $@; \
		echo "    \"command\": \"$$cmd\"," >> $@; \
		echo "    \"file\": \"$$dir/$$file\"" >> $@; \
		echo "  }," >> $@; \
	done
	@sed -i '$$s/},$$/}/' $@
	@echo "]" >> $@
	@echo "compile_commands.json generated"

# Clean build artifacts
clean:
	$(RM) -r $(BUILD_DIR) $(LIB_DIR) compile_commands.json

# Install the library to system directory (optional)
install: $(TARGET)
	@echo "Installing library to /usr/local/lib"
	cp $(TARGET) /usr/local/lib/
	ldconfig

# Uninstall library (optional)
uninstall:
	$(RM) /usr/local/lib/libiAP2.so
	ldconfig

# Show information about the build
info:
	@echo "Source directories: $(SRC_DIRS)"
	@echo "C++ source files:"
	@for file in $(CPP_FILES); do echo "  $$file"; done
	@echo "C source files:"
	@for file in $(C_FILES); do echo "  $$file"; done
	@echo "Total source files: $(words $(SRC_FILES))"
	@echo "Object files will be placed in: $(OBJ_DIR)"
	@echo "Library will be created as: $(TARGET)"
	@echo "Include directories: $(INCLUDE_DIRS)"
	@echo "Compiler flags: $(CXXFLAGS)"

# Phony targets
.PHONY: all clean install uninstall info

# Dependency generation (optional but recommended for large projects)
DEPS := $(OBJ_FILES:.o=.d)

# Include dependencies if they exist
-include $(DEPS)

# Rule to generate dependency files
$(OBJ_DIR)/%.d: %.cpp | $(OBJ_DIR)
	@$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $< -o $@

$(OBJ_DIR)/%.d: %.c | $(OBJ_DIR)
	@$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) $< -o $@

