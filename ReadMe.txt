The Neural Database
(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.


Catalog of the 22 files in this distribution:

		LICENSE - Open Source

	Introduction to the Neural Database
		Ndb_Document.pdf
		Ndb_Supplementary_Information.pdf

	The Neural Database application
		Ndb64.exe - Windows 11, Win64 Console

	Source Code
		ndb.h - runtime parameters, data structures, links, etc.
		NdbTest.c - menu options in the Ndb application
		NdbRecognize.c - inquiry calls, boundary adjusters, creates branches, SCU tournament 
		NdbGetONs.c - RN-to-ON scan, define Bound Sections, initial thresholds
		NdbSCU.c - run SCU competition between 2 branches
		NdbCreate.c - make Ndb databases from TXT files
		NdbCreateImage.c - convert MNIST image into RNs, make 'image' Ndb databases
		NdbLoad.c - load an Ndb database (*.ndb) into memory
		NdbActions.c - executable code to answer the questions in Questions.txt
	
	Data Files
		ReadMe.txt
		Questions.txt - the questions to be stored in database Ndb #50, to be linked from Ndb #1
		WordMin.txt - 86 English words making up the most likely results in TestLib.txt
		WordMax.txt - 96,000+ English words
		TestLib.txt - 160+ test cases, each with a list of acceptable results

		The MNIST Training set of handwritten digits and the MNIST Test set
		(from http://yann.lecun.com/exdb/mnist/)
			mnist_train_1.txt - the first 20,000 images
			mnist_train_2.txt - the middle 20,000 images
			mnist_train_3.txt - the last 20,000 images
			mnist_test.txt - 10,000 images


A. Overview:
The Neural Database application, Ndb64.exe, runs on MS Windows.
All of the above files should be loaded into directory C:\Ndb
Neural databases will be created in subdirectory: C:\Ndb\ndbdatabases
If the subdirectory doesn't exist, the application will create it.

The Ndb application demonstrates running inquiries into Neural Databases. It is written
in C, compiled under Windows 11 with Pelles C (see below), and will launch a Win64 Console.
Any modern C compiler should be configurable to create a new Ndb application from the
source code in these distribution files.

This implementation is not a full-featured database nor a high-performance storage system.
It has a crude user interface, gets its data from simple text files, and the source code is
not meant to be an example of good programming practices. The Ndb64.exe application is a
research tool, not a product. 

Neural networks burn the cpu during training, but once trained they can have a very fast
response time. The Neural Database is the opposite - it's very fast at storing data but
burns the cpu running inquires. Multiprocessing via OpenMP is used for the most time
consuming functions. Running on a fast processor with lots of cores is a good idea. If
you have a processor with more than 128 available threads, you can access them by editing
the value of MAX_THREADS in ndb.h and recompiling. The application will use as many threads
as the operating system will give it.

The application was developed and tested on this system:
	Windows 11 (64-bit) Intel i9-12900HK (20 threads), 2.92 GHz, 16 GB

Using this hardware, the most time-consuming operations are:
	Running the Test Library on Ndb #12 (96,000+ words)		3.25 hours
	Creating 399 Ndb's from the 60,000 MNIST Training images	12 minutes
	Running all 10,000 MNIST Test images				7.5 hours


B. Compiling with Pelles C, version 10.00.6:
	1) Download and install Pelles C from www.smorgasbordet.com/pellesc/
	2) Launch the "Pelles C IDE"
	3) In the Start Page, select "Start a new project"
	4) For project type, under "Empty projects" select the "Win64 Console program"
		Give it whatever name you like
		Location: "C:\Ndb"
	5) In the IDE, select: Project
			 Project options...
				General
					Folders
						Type: Select "Libraries" and then "Includes"
							For each of these, click on New Folder
								Browse to folder C:\Ndb
									Click on OK to add the folder
	6) Enable OpenMP:
	   In the IDE, select: Project
			 Project options...
				Compiler
					Check the box: Enable OpenMP extensions
				Compiler
					Code generation
						Runtime library - select: Multithreaded (LIB)
				Linker
					Add this file to the "Library and object files:"
						pomp64.lib


C. Menu Options in Ndb64.exe:
   The code for all of these Options is in NdbTest.c

Menu Option #1 - Create Ndb's #1, #2, ... #12 from WordMin.txt and WordMax.txt

	This will create 12 Ndb’s of ever-increasing size from 86 to more than 96,000 English
	words. You will have the option of entering an ‘offset’ which will give you a different
	mix of words in the intermediate databases (#2 through #11). The databases will be
	created in C:\Ndb\ndbdatabases\ and will be named 1.ndb, 2.ndb, ... 12.ndb.

	All Neural Databases in this implementation are actually text files which can be viewed
	with any editor. It would be unwise to actually edit an *.ndb file.

	The application will load the *.ndb file into memory structures whenever an inquiry
	into the database is made (Options #5 - #12).


Menu Option #2 - Create Ndb #N from a text file of your choice

	You can create your own word-recognizing Neural Database with this option and assign it
	a unique number. Refer to WordMin.txt for the format of your text file.


Menu Option #3 - Create a 'CENTRAL' Ndb (#50) with executable Actions from Questions.txt

	Select this option to create 50.ndb - a Neural Database with some basic questions.
	All the questions depend entirely on the words in 1.ndb and the linkage from 1.ndb to
	50.ndb is tested below in Option #9. NdbActions.c contains the code for the Actions.


Menu Option #4 - Create 399 Ndb's (#2000 through #2569) from the MNIST Training Set

	This option will use the 60,000 MNIST training images in the 3 mnist_train_*.txt files
	to create the 399 Ndb’s needed for the recognition of MNIST Test images.

	These 'image' databases are numbered 2000.ndb to 2569.ndb and, as usual, will be
	created in C:\Ndb\ndbdatabases. Although it's unimportant, these databases are not
	numbered sequentially. Why? Development history, for-loop conveniences, stuff like that.


Menu Option #5 - Inquire into Ndb #N

	This option allows you to test any word-recognizing Ndb of your choice with any text
	you choose to enter. You can also specify whether or not you want to see the internal
	processing functions displayed as they are performed.

	If you don't get the response you were expecting, consider the following:

		1) The Neural Database does not know the meaning or context of anything. It is
		   simply a recognition system. If you entered ONDAY, is that a misspelling of
		   the word MONDAY, or the two words ON DAY? Both are equally valid. If however
		   you put the space between the two words, then a response of MONDAY would
		   be an indication of a bug in the system, unless ON and/or DAY are not even
		   in the database you're testing.

		2) You should also take into consideration the level of corruption in your
		   inquiry. If a human being can't figure it out, the Ndb probably won't
		   either.

		3) A bizarre result, such as entering FRODAY, expecting FRIDAY for the response,
		   but instead you got FRIED GO HAY, would be indicative of a bug in the system.

			a) Knowing the sequence of functions that are called, which you can
			   specify when you use this menu option, you can start with the call
			   to CompeteBranches() to see if FRIDAY even made it into the list of
			   branches. If it did then there's a problem with the SCU Agents and
			   the spike trains. Changing the number of spike calls, or adding a new
			   Agent based on some new measurement, might solve the problem.

			b) If FRIDAY didn't even make it into the competitive branches, then one
			   of the thresholds filtered it out and may need to change, or one of
			   the boundary adjustment functions may have mistakenly removed it and
			   the code and/or parameters in the function may need to be fixed.

			c) No problem in the Neural Database is actually solved until the change
			   is thoroughly tested on many different databases with a robust Test
			   Library.
 

Menu Option #6 - Run an inquiry on all the Ndb's: #1, #2, ... #12

	This is the extreme version of Option #5, and the inquiry will take longer as the databases
	get bigger.


Menu Option #7 - Run the Test Library (TestLib.txt) on Ndb #N

	The test library contains 160+ examples of text, some corrupted in some way and some
	not so much. A Neural Database should be able to recognize both the good and the bad.
	After selecting the database you want to test, you'll get 3 choices to:
		1 - display ALL results (so you can watch the progress), or
		2 - display only results with errors, or
		3 - just display the final error count

	You can create your own test libraries by editing or replacing TestLib.txt. Multiple
	interpretations of corrupted text should be expected, especially when inquiring into
	large databases. TestLib.txt has examples of multiple correct results.


Menu Option #8 - Run the Test Library on all the Ndb's: #1, #2, ... #12

	This is the extreme version of Option #7, and the set of tests will take longer as the
	databases get bigger.


Menu Option #9 - Test 'linked' Ndb's: Ndb #1 --> Ndb #50 --> execute Action

	This option lets you ask questions of the Ndb. The questions must be similar to those
	listed in Questions.txt, which supplies the data stored by Option 3 in Ndb #50. The
	Ndb will attempt to recognize the question you are asking and respond with one of the
	Actions listed in NdbActions.c. Since the linkage is only with Ndb #1, this option is
	only capable of recognizing the words stored in Ndb #1.


Menu Option #10 - Recognize a specific MNIST Test image using the image Ndb's

	After having created all the image Ndb's via Option #4, you can enter the record number
	of any of the 10,000 images in the MNIST Test Set (mnist_test.txt) to test the Ndb's
	ability to recognize the hand-written digits.


Menu Option #11 - Run all 10,000 MNIST Test Set images on the image Ndb's

	This option will start the process of recognizing all the test images. Any image which
	fails to be correctly identified will be listed in the file C:\Ndb\mnist_errors.txt.


Menu Option #12 - Add failures from Option 11 (mnist_errors.txt) to the image Ndb's

	This option will take the images that failed in Option #11 and add them to the image Ndb’s
	as a demonstration of the Neural Database’s ability to be rapidly updated with new information,
	i.e. to rapidly learn from mistakes.


Menu Options #13 to #21 - Test the Ndb with various SCU spike trains turned ON/OFF

	Prior to running any of the Options #5 through #11, these options allow you to alter the
	processing of competitions in the Simple Competitive Unit (SCU) by switching any or all
	of the SCU spike trains ON or OFF.
End of File