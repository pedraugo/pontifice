# Converte um arquivo de texto em lista de bytes C (0x..,) para ser incluído em um char[].
# Uso: cmake -DIN=arquivo -DOUT=saida.inc -P embed.cmake
file(READ "${IN}" HEX_CONTENT HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BYTES "${HEX_CONTENT}")
file(WRITE "${OUT}" "${BYTES}0x00\n")
