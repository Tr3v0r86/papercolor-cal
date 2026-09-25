# Host test for cal_model against IDF's bundled cJSON. Run: make test
IDF   ?= $(HOME)/esp/esp-idf-v5.5.1
CJSON := $(IDF)/components/json/cJSON
CFLAGS := -std=c99 -Wall -Wextra -Werror -I$(CJSON) -I../../main/cal

test: test_cal_model
	./test_cal_model sample.json

test_cal_model: test_cal_model.c ../../main/cal/cal_model.c ../../main/cal/cal_model.h
	$(CC) $(CFLAGS) -o $@ test_cal_model.c ../../main/cal/cal_model.c $(CJSON)/cJSON.c

clean:
	rm -f test_cal_model

.PHONY: test clean
