import sys, os

str = raw_input("Enter the timestamp of your log: ")

LOG_DIR = "logs"
LOG_NAME = "log" + str + ".txt"
TEMP_LOG_NAME = "temp" + str + ".csv"
PRES_LOG_NAME = "pres" + str + ".csv"

#Convert the text file to the appropriate file type

try:
##    fp = open(LOG_NAME)
##    csvtp = open(TEMP_LOG_NAME, 'w')
##    csvpp = open(PRES_LOG_NAME, 'w')

    fp = open(LOG_DIR + '/' + LOG_NAME)
    csvtp = open(LOG_DIR + '/' + TEMP_LOG_NAME, 'w')
    csvpp = open(LOG_DIR + '/' + PRES_LOG_NAME, 'w')

    csvtp.write("\"Timestamp\",\"Temperature\"\n")
    csvpp.write("\"Timestamp\",\"Pressure\"\n")
    for line in fp:
        splitLine = line.partition(",")
        if splitLine[0] == "ADC":
            csvtp.write(splitLine[2].rstrip("\n"))
        if splitLine[0] == "TWI":
            csvpp.write(splitLine[2].rstrip("\n"))
            
    csvpp.close()
    csvtp.close()
    fp.close()
    
except IOError:
    print("The file was not found.")
