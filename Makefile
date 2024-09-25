TARGET = wayvibes
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	g++ -o $(TARGET) $(SRC) -levdev --verbose
		@echo -e "\nRun this command to install wayvibes: \n \
		\033[0;32m\tsudo make install\033[0m"

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	@echo -e "\033[0;32mInstalltion completed.\033[0m \n\n \
		Add user to the 'input' group by the following command: \n \
		\033[0;32m\tsudo usermod -a -G input <your_username>\033[0m \n \
		Then Reboot or Logout and Login for the changes to take effect. \n \
	       (Ignore if already done)\n\n \
		Run the application: \n \
		\033[0;32m\twayvibes <soundpack_path> -v <volume(0.0-10.0)>\033[0m \n \
		Example: \n \
		\033[0;32m\twayvibes ~/wayvibes/akko_lavender_purples/ -v 3\033[0m"

clean:
	rm -f $(TARGET)

.PHONY: all install clean
