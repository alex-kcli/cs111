default: 
	gcc -g -Wall -Wextra lab0.c -o lab0

check:
	echo "Normal Test: "; \
	touch input.txt; \
	echo "Sample input" >> input.txt; \
	touch output.txt; \
	./lab0 --input="input.txt" --output="output.txt"; \
	if [ $$? -eq 0 ]; then \
		echo "Program successfully exited"; \
	else \
		echo "Error: Program exited with wrong error code $$?, correct code: 0"; \
		exit 1; \
	fi; \

	cmp input.txt output.txt 2> /dev/null; \
	if [ $$? -eq 0 ]; then \
		echo "Program copied the correct information"; \
	else \
		echo "Error: Program failed to copy the correct information"; \
		exit 1; \
	fi; \



	echo "---------------"; \
	echo "Wrong Option Usage Test:"; \
	./lab0 --wrongoption 2> /dev/null; \
	if [ $$? -eq 1 ]; then \
		echo "Program successfully exited with exit code 1"; \
	else \
		echo "Error: Program exited with wrong error code $$?, correct code: 1"; \
		exit 1; \
	fi; \



	echo "---------------"; \
	echo "Input File Test: "; \
	chmod 000 input.txt; \
	./lab0 --input="input.txt" 2> /dev/null; \
	if [ $$? -eq 2 ]; then \
		echo "Program successfully exited with exit code 2"; \
	else \
		echo "Error: Program exited with wrong error code $$?, correct code: 2"; \
		exit 1; \
	fi; \

	chmod 644 input.txt;
	chmod 644 output.txt;



	echo "---------------"; \
	echo "Output File Test: "; \
	chmod 600 input.txt; \
	chmod 000 output.txt; \
	./lab0 --output="output.txt" 2> /dev/null; \
	if [ $$? -eq 3 ]; then \
		echo "Program successfully exited with exit code 3"; \
	else \
		echo "Error: Program exited with wrong error code $$?, correct code: 3"; \
		exit 1; \
	fi; \

	chmod 644 input.txt; \
	chmod 644 output.txt; \



	echo "---------------"; \
	echo "Segmentational Fault Test: "; \
	./lab0 --segfault --catch 2> /dev/null; \
	if [ $$? -eq 4 ]; then \
		echo "Program successfully exited with exit code 4"; \
	else \
		echo "Error: Program exited with error code $$?, correct code: 4"; \
		exit 1; \
	fi; \

	rm input.txt; \
	rm output.txt; \
	echo "Passed all smoke test cases, exiting ... "


clean:
	rm -f *.tar.gz lab0

dist:
	tar -czf lab0-205366842.tar.gz lab0.c README Makefile backtrace.png breakpoint.png