#include "format/file_processor.h"
#include "format/vulkan_ascii_consumer.h"
#include "format/vulkan_decoder.h"
#include "util/argument_parser.h"

void PrintUsage(const char* exe_name)
{
    std::string app_name     = exe_name;
    size_t      dir_location = app_name.find_last_of("/\\");
    if (dir_location >= 0)
    {
        app_name.replace(0, dir_location + 1, "");
    }
    printf("\n\n%s\tis a trace replay tool designed to output the API commands\n", app_name.c_str());
    printf("\t\tfound inside of a trace binary file.\n\n");
    printf("Usage:\n");
    printf("\t%s <binary_file>\n\n", app_name.c_str());
    printf("\t<binary_file>\t\tThe filename (including path if necessary) of the trace\n");
    printf("\t\t\t\tbinary file whose API command contents should be outputted.\n");
    printf("\t\t\t\tThe results will be output to a file of the same name but\n");
    printf("\t\t\t\twith \'.bin\' suffix replaced with \'.txt\'.\n");
}

int main(int argc, const char** argv)
{
    brimstone::format::FileProcessor file_processor;
    std::string                      bin_file_name = "brimstone_test.bin";
    brimstone::util::ArgumentParser  arg_parser(argc, argv, "", "", 1);

    const std::vector<std::string> non_optional_arguments = arg_parser.GetNonOptionalArguments();
    if (arg_parser.IsInvalid() || non_optional_arguments.size() != 1)
    {
        PrintUsage(argv[0]);
        exit(-1);
    }
    else
    {
        bin_file_name = non_optional_arguments[0];
    }

    if (argc > 1)
    {
        bin_file_name = argv[1];
    }

    std::string text_file_name = bin_file_name;
    size_t      suffix_pos     = text_file_name.find(".bin");
    if (suffix_pos == std::string::npos)
    {
        text_file_name += ".txt";
    }
    else
    {
        text_file_name.replace(suffix_pos, 4, ".txt");
    }

    if (file_processor.Initialize(bin_file_name))
    {
        brimstone::format::VulkanDecoder       decoder;
        brimstone::format::VulkanAsciiConsumer ascii_consumer;

        ascii_consumer.Initialize(text_file_name);
        decoder.AddConsumer(&ascii_consumer);

        file_processor.AddDecoder(&decoder);
        file_processor.ProcessAllFrames();
    }

    return 0;
}
