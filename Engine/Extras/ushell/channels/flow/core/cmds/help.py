# Copyright Epic Games, Inc. All Rights Reserved.

import re
import sys
import shutil
import marshal
import flow.cmd
import textwrap
import importlib
from pathlib import Path

#-------------------------------------------------------------------------------
class Help(flow.cmd.Cmd):
    """ Prints information about the available commands """

    def main(self):
        channel = self.get_channel()
        system = channel.get_system()
        cmd_tree = system.get_command_tree()
        root_node = cmd_tree.get_root_node()

        def read_invokeables(node, path=""):
            if node.is_invokeable():
                yield path, node

            for child_name, child_node in node.read_children():
                child_path = (path + " " + child_name) if path else child_name
                yield from read_invokeables(child_node, child_path)

        def read_output():
            for path, node in read_invokeables(root_node):
                if path.startswith("$"):
                    continue

                try:
                    command_class = node.get_command_class()
                    command_class = command_class.get_class_type()
                    if hasattr(command_class, "get_desc"):
                        yield path, command_class.get_desc()
                except Exception as e:
                    yield path, "LOAD_ERROR: " + str(e)

        right_column = shutil.get_terminal_size()[0]
        right_column -= 5

        output = list(read_output())
        output.sort(key=lambda x: x[0])

        name_len = max(len(x[0]) for x in output) + 4
        desc_width = right_column - name_len
        for name, desc in output:
            dots = "." * (name_len - len(name) - 3)
            initial_prefix = flow.cmd.text.light_cyan(name) + " " + flow.cmd.text.grey(dots)

            is_first_paragraph = True

            paragraph_lines = []
            is_literal_block = None

            def print_paragraph(is_literal_block, is_first_paragraph):
                if paragraph_lines:
                    # literal blocks retain line endings and wrap on indent
                    if is_literal_block:
                        desc_lines = []
                        for line in paragraph_lines:
                            line_len = len(line)
                            line = line.lstrip()
                            line_indent = line_len - len(line)

                            desc_lines.extend([(" " * line_indent) + l for l in textwrap.wrap(line, desc_width)])
                    else:
                        desc_lines = textwrap.wrap(" ".join(paragraph_lines), desc_width)

                    paragraph_prefix = (" " * (name_len - 2))

                    prefix = initial_prefix if is_first_paragraph else paragraph_prefix
                    print("", prefix, desc_lines[0])

                    for line in desc_lines[1:]:
                        print("", paragraph_prefix, line)
                    
                    del paragraph_lines[:]

                    if desc_lines:
                        print()

            for desc_line in desc.splitlines():
                if not desc_line:
                    print_paragraph(is_literal_block, is_first_paragraph)
                    is_first_paragraph = False
                    is_literal_block = None
                else:
                    # treat paragraph as a literal block if it starts out indented
                    if is_literal_block is None:
                        is_literal_block = desc_line[0].isspace()

                    paragraph_lines.append(desc_line)

            print_paragraph(is_literal_block, is_first_paragraph)



#-------------------------------------------------------------------------------
class ReadMe(flow.cmd.Cmd):
    """ Opens the README.txt file in your editor of choice or prints it to stdout """
    editor = flow.cmd.Opt(False, "Open in a text editor")

    def main(self):
        for haystack_dir in Path(__file__).parents:
            readme_path = haystack_dir / "README.txt"
            if readme_path.is_file():
                break
        else:
            self.print_error("Unable to find README.txt")
            return False

        if self.args.editor:
            self.edit_file(readme_path)
            return

        with readme_path.open("rt") as file:
            for line in file.readlines():
                line = line.strip()
                if line.startswith("#"):
                    with flow.cmd.text.light_cyan:
                        print(line)
                else:
                    print(line)
