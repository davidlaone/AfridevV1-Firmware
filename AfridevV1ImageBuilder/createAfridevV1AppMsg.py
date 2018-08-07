#
# This utility is used to create the afridevV1 Application upgrade message 
# using the afridevV1 MSP430 output build file CCS_AfridevV1_MSP430.txt.
#
# It also combines the Application output text file and Boot output
# text file into a single file that can be used by the FET programmer
# to burn the flash.
#
# This utility assumes its running from a directory parallel to the 
# MSP430 Application Project and MSP430 Boot Project.
#
#
#

import subprocess
import os.path
import shutil
import re

cwd = os.getcwd()
# print (cwd)

# Identify the Application and Boot files
InputAppOutFileName = cwd + '/' + '../AfridevV1_MSP430/CCS_AfridevV1_MSP430/Debug/CCS_AfridevV1_MSP430.out'
InputBootOutFileName = cwd + '/' + '../Outpour_Boot_MSP430/Debug/Outpour_Boot_MSP430.out'
InputAppTxtFileName = cwd + '/' + '../AfridevV1_MSP430/CCS_AfridevV1_MSP430/Debug/CCS_AfridevV1_MSP430.txt'
InputBootTxtFileName = cwd + '/' + '../Outpour_Boot_MSP430/Debug/Outpour_Boot_MSP430.txt'

# print (InputAppOutFileName)

# ==============================================================
# Run the hex430.exe file to build the ROM output file first.
# The ROM file is used to create the Upgrade Firmware Message.
#

# Make sure the MSP430 application out file is present
fileFound = os.path.exists(InputAppOutFileName)
if fileFound == False:
    print ('Could not locate application out file')
    exit()
else:
    print ('Application out file found')
    shutil.copy (InputAppOutFileName, cwd);

# Run the hex430 to create the rom file from the MSP app out file
# C:\ti\ccsv7\tools\compiler\ti-cgt-msp430_16.9.1.LTS\bin\hex430.exe outpour_app_to_rom.cmd
# The output file produced by the hex430.exe utility is: afridevV1_MSP430_rom.txt
p = subprocess.Popen([ 'hex430.exe', 
                       'AfridevV1_app_to_rom.cmd' ], 
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
# Dump output from subprocess standard output as it runs (i.e. output from hex430.exe)
for line in p.stdout.readlines():
    print (line)
retval = p.wait()

# Create the Application Upgrade Message File
p = subprocess.Popen([ 'python', 
                       'afridevV1RomToMsg.py', 
                       'afridevV1_MSP430_rom.txt', 
                       'afridevV1_MSP430_msg.txt' ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
# Dump output from subprocess standard output as it runs (i.e. output from python script)
for line in p.stdout.readlines():
    print (line)
retval = p.wait()

fileFound = os.path.exists('afridevV1_MSP430_msg.txt')
if fileFound == False:
    print ('afridevV1_MSP430_msg.txt file not created.')
    exit()
else:
    print ("File afridevV1_MSP430_msg.txt created.")

# ==============================================================
# -Combine the application file and the boot file
#  into a single txt file.
# -The combination is created using the two build output text
#  files
#   + AfridevV1_MSP430/CCS_AfridevV1_MSP430/Debug/CCS_AfridevV1_MSP430.txt'
#   + Outpour_Boot_MSP430/Debug/Outpour_Boot_MSP430.txt'
# -We need to remove the line with the "q" at the end of the
#  Application output text file when we combine the two files.
#

# Make sure the MSP430 boot out file is present
fileFound = os.path.exists(InputBootOutFileName)
if fileFound == False:
    print ('Could not locate boot out file')
    exit()
else:
    print ('Boot file found')

# Create the combined boot file and Application file
# destination = open('afridevV1_App_Boot_MSP430.txt','wb')
# shutil.copyfileobj(open(InputAppTxtFileName,'rb'), destination)
# shutil.copyfileobj(open(InputBootTxtFileName,'rb'), destination)
# destination.close()

# Create a regex to ignore any line start with a "q".
reFilterTxtFile = re.compile("^[^q]")
# Open the rom file and get rid of any non-code lines.
txtFileString = ""
f = open(InputAppTxtFileName, 'r')
for line in f:
    if reFilterTxtFile.match(line) != None:
        txtFileString+=line
f.close()
f = open(InputBootTxtFileName, 'r')
for line in f:
        txtFileString+=line
f.close()
f = open('afridevV1_App_Boot_MSP430.txt', 'w')
f.write (txtFileString)
f.close()

fileFound = os.path.exists('afridevV1_App_Boot_MSP430.txt')
if fileFound == False:
    print ('afridevV1_App_Boot_MSP430.txt file not created.')
    exit()
else:
    print ("File afridevV1_App_Boot_MSP430.txt created.")

# ==============================================================
