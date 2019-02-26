import argparse
import re
import os
import os.path
from os.path import join
import subprocess

def sed(content, regex, repl):
    return re.sub(regex, repl, content, flags = re.MULTILINE | re.DOTALL)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage="migrate sg tex file")
    parser.add_argument("filename", help="")
    parser.add_argument("output_dir", help="")
    args = parser.parse_args()

    input = args.filename
    output = join(args.output_dir, os.path.basename(args.filename).replace(".tex", ".rst"))

    content = open(input).read()

    content = sed(content,
            r"\\doxygen\{otb\}\{(.*?)\}",
            r":doxygen:`\1`")

    content = sed(content,
            r"\\doxygen\{itk\}\{(.*?)\}",
            r":doxygen-itk:`\1`")

    content = sed(content,
            r"\\code\{(.*?)\}",
            r"\\texttt{\1}")

    open(output, "w").write(content)

    subprocess.check_call("pandoc -f latex -t rst -o {} {}".format(output, output), shell=True)
    subprocess.check_call(['sed', '-i', "s ‘ ` g", output])
    print(output)
