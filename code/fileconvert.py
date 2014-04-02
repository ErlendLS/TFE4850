import sys, os

LOG_DIR = "logs"
LOG_NAME = "log.txt"
TEMP_LOG_NAME = "temp.csv"
PRES_LOG_NAME = "pres.csv"

if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)
#Convert the text file to the appropriate file type
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
