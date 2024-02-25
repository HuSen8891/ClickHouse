#include <Formats/FormatFactory.h>
#include <IO/WriteBuffer.h>
#include <IO/WriteHelpers.h>
#include <Processors/Formats/Impl/PrettySpaceBlockOutputFormat.h>
#include <Common/PODArray.h>


namespace DB
{


void PrettySpaceBlockOutputFormat::writeChunk(const Chunk & chunk, PortKind port_kind)
{
    UInt64 max_rows = format_settings.pretty.max_rows;

    if (total_rows >= max_rows)
    {
        total_rows += chunk.getNumRows();
        return;
    }

    size_t num_rows = chunk.getNumRows();
    size_t num_columns = chunk.getNumColumns();
    const auto & header = getPort(port_kind).getHeader();
    const auto & columns = chunk.getColumns();
    auto single_number_value = num_rows == 1 && num_columns == 1 && WhichDataType(columns[0]->getDataType()).isNumber();

    WidthsPerColumn widths;
    Widths max_widths;
    Widths name_widths;
    calculateWidths(header, chunk, widths, max_widths, name_widths);

    if (format_settings.pretty.output_format_pretty_row_numbers)
        writeString(String(row_number_width, ' '), out);
    /// Names
    for (size_t i = 0; i < num_columns; ++i)
    {
        if (i != 0)
            writeCString("   ", out);
        else
            writeChar(' ', out);

        const ColumnWithTypeAndName & col = header.getByPosition(i);

        if (col.type->shouldAlignRightInPrettyFormats())
        {
            for (ssize_t k = 0; k < std::max(0z, static_cast<ssize_t>(max_widths[i] - name_widths[i])); ++k)
                writeChar(' ', out);

            if (color)
                writeCString("\033[1m", out);
            writeString(col.name, out);
            if (color)
                writeCString("\033[0m", out);
        }
        else
        {
            if (color)
                writeCString("\033[1m", out);
            writeString(col.name, out);
            if (color)
                writeCString("\033[0m", out);

            for (ssize_t k = 0; k < std::max(0z, static_cast<ssize_t>(max_widths[i] - name_widths[i])); ++k)
                writeChar(' ', out);
        }
    }
    writeCString("\n\n", out);

    for (size_t row = 0; row < num_rows && total_rows + row < max_rows; ++row)
    {
        if (format_settings.pretty.output_format_pretty_row_numbers)
        {
            // Write row number;
            auto row_num_string = std::to_string(row + 1 + total_rows) + ". ";
            for (size_t i = 0; i < row_number_width - row_num_string.size(); ++i)
                writeCString(" ", out);
            writeString(row_num_string, out);
        }
        for (size_t column = 0; column < num_columns; ++column)
        {
            if (column != 0)
                writeCString(" ", out);

            const auto & type = *header.getByPosition(column).type;
            auto & cur_width = widths[column].empty() ? max_widths[column] : widths[column][row];
            writeValueWithPadding(
                *columns[column], *serializations[column], row, cur_width, max_widths[column], type.shouldAlignRightInPrettyFormats());
        }

        if (single_number_value)
        {
            auto value = columns[0]->getFloat64(0);
            if (value > 1'000'000)
                writeReadableNumberTip(value);
        }
        writeChar('\n', out);
    }

    total_rows += num_rows;
}


void PrettySpaceBlockOutputFormat::writeSuffix()
{
    writeMonoChunkIfNeeded();

    if (total_rows >= format_settings.pretty.max_rows)
    {
        writeCString("\nShowed first ", out);
        writeIntText(format_settings.pretty.max_rows, out);
        writeCString(".\n", out);
    }
}


void registerOutputFormatPrettySpace(FormatFactory & factory)
{
    registerPrettyFormatWithNoEscapesAndMonoBlock<PrettySpaceBlockOutputFormat>(factory, "PrettySpace");
}

}
