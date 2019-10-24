#!/usr/bin/env python

import os, re, sys

# this script will append to targetHeaderFile a bunch of lines such as 

# UNLINK_VERSION = GLIBC_2.1

# based on inspecting the output of running ldd on the linkingHelperBinary,
# then running readelf on the libraries listed there and, finally, extracting
# the most recent symbol version from the latter's output.

# Returns 0 in case of success, 1, otherwise. (libtrash should NOT be built
# if this script returns 1.)

# CONFIG (ALL THESE MUST BE SET CORRECTLY) =============

linkingHelperBinary = "linking-helper" # name of binary to run ldd on; this binary should invoke all functions listed in tagetGlibcFunctions (below)

targetGlibcFunctions = ["unlink", "unlinkat", "rename", "renameat", \
"fopen", "fopen64", "freopen", "freopen64", "open", "openat", "open64", "openat64", "creat", "creat64"] # these are the functions we want to get the symbol versions for

atFunctions = ["unlinkat", "renameat", "openat", "openat64"] # subset of targetGlibcFunctions which belong to the more recent (and not supported on every system), *at() family

targetHeaderFile = "trash.h" # this is the header file to which we will append lines of the form FUNCTION_VERSION = "SYMBOL_VERSION"

re_ldd_output = re.compile("\s*(\S+)(?: => (\S+))?.*")

re_readelf_output = re.compile(".*\s+(.*)@@(.*)")

# CODE ===========================

def run_cmd(cmd, *args):
    
    rewritten_cmd = [cmd] # cmd does not get escaped so that we _can_ use shell globbing by just passing a command as a single string
    
    rewritten_cmd += [re.escape(i) for i in args]
    
    cmd_str = " ".join(rewritten_cmd)
    
    proc = os.popen(cmd_str, "r")
    
    output = proc.read()
    status = proc.close()
    
    if status == None: return ("0", output) # os.popen() returns None if exit code == 0
    
    if os.WIFEXITED(status):
        
        return str(os.WEXITSTATUS(status)), output

    else:
        
        return "EXITED ABNORMALLY", output

    pass


def main():
    
    libsToInspect = []
    
    functionToVersionDic = {} # this is the funcname -> symbolversion mapping which will go into the header file
    
    for fun in targetGlibcFunctions: functionToVersionDic[fun] = None
    
    enableSupportForAtFunctions = True
    
    # first, get the list of libsToInspect
    
    s,o = run_cmd("ldd", "linking-helper")
    
    if s != "0":
        
        print("couldn't run ldd on linking-helper!")
        print(str(o))
        sys.exit(1)
        pass
    
    lines = o.split("\n")
    
    for line in lines:
        
        m = re.match(re_ldd_output, line)
        
        if m != None and len(m.groups()) == 2 and m.group(2) != None: 
            
            libsToInspect.append(m.group(2))
            pass
        
        pass
    
    # now, for each lib look for the default symbol for each of the targetGlibcFunctions and (IF 
    # THEY HAVEN'T BEEN DEFINED IN AN EARLIER LIB) add them to functionToVersionDic
    
    for lib in libsToInspect:
        
        s, o = run_cmd( "readelf" , "-s", lib)
        
        if s != "0": 
            
            print("couldn't run readelf -s %s!"%lib)
            print(str(o))
            sys.exit(1)
            pass
        
        lines = o.split("\n")
        
        # inspect output of running readelf on this lib, storing all relevant mappings
        
        for line in lines:
            
            match = re_readelf_output.match(line) # group(1) is function name, group(2) is symbol version
            
            if match != None and match.group(1) in targetGlibcFunctions and \
            functionToVersionDic[match.group(1)] == None:
                
                functionToVersionDic[match.group(1)] = match.group(2)
                pass
            pass
        
        pass
    
    
    # since not all systems support them (they are quite recent), we simply ignore the atFunctions for which we couldn't
    # get symbol versions
    
    for atFunction in atFunctions:
        
        if functionToVersionDic[atFunction] == None:
            
            print( "WARNING: didn't find a symbol for %s"%atFunction)
            
            if enableSupportForAtFunctions:
                
                enableSupportForAtFunctions = False
                print( "=> DISABLING LIBTRASH SUPPORT FOR 'AT FUNCTIONS' SINCE IT DOESN'T LOOK LIKE YOUR SYSTEM SUPPORTS THEM")
                print( "=> YOU CAN SAFELY IGNORE THESE WARNINGS ABOUT MISSING SYMBOLS")
                pass
            
            del functionToVersionDic[atFunction]
            pass
        
        pass
    
    
    # make sure that we got a version for every symbol we were interested in
    
    if None in functionToVersionDic.values():
        
        print( "MISSING SYMBOL VERSION FOR AT LEAST ONE FUNCTION!")
        for k,v in sorted(functionToVersionDic.items(), key=lambda i: i[0]) : print( "%s => %s"%(k,v))
        sys.exit(1)
        pass
    
    #for k,v in sorted(functionToVersionDic.items(), key=lambda i: i[0]) : print( "%s => %s"%(k,v)) # DEBUG
    
    # open header file and append the correct macros to it
    
    headerFile = open(targetHeaderFile, "a") # the automatically-generated section of the header file was already started by the genheader.pl script, which the makefile runs before get_symbol_versions.py
    
    headerFile.write("\n")
    
    if enableSupportForAtFunctions: headerFile.write("#define AT_FUNCTIONS\n\n")
    
    for k,v in sorted(functionToVersionDic.items(), key=lambda i: i[0]) : 
        
        headerFile.write("#define %s_VERSION \"%s\"\n"%(k.upper(),v))
        pass
    
    headerFile.write("\n/* END OF AUTOMATICALLY-GENERATED CONFIGURATION SECTION */\n") # the corresponding BEGINNING header was already in place (added by the Perl script genheader.pl)
    headerFile.close()
    return



# check that we are running a sufficiently recent version of Python

if not \
(sys.version_info[0] > 2 or \
(sys.version_info[0] == 2 and sys.version_info[1] >= 5)):
    
    print( "You need to have Python >= 2.5 installed!")
    sys.exit(1)
    pass

main()
