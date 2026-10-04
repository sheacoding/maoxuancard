/**
 * [INPUT]: 设备返回的 L2 黑/红双平面位图
 * [OUTPUT]: 对外提供 250×122 RGBA 解码和尺寸常量
 * [POS]: 网页预览的二进制边界；位序与固件 core.h 一致
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
export const WIDTH = 250, HEIGHT = 122, STRIDE = Math.ceil(WIDTH / 8);
const HEADER = new TextEncoder().encode(`L2\n${WIDTH} ${HEIGHT}\n`);
const PLANE = STRIDE * HEIGHT;

export function decodeFrame(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length !== HEADER.length + PLANE * 2 ||
      !HEADER.every((value, index) => bytes[index] === value))
    throw new Error('设备彩色位图格式不正确。');
  const rgba = new Uint8ClampedArray(WIDTH * HEIGHT * 4);
  for (let y = 0; y < HEIGHT; ++y) {
    const last = HEADER.length + y * STRIDE + STRIDE - 1;
    if ((bytes[last] | bytes[last + PLANE]) & 0x3f) throw new Error('位图行尾补位错误。');
    for (let x = 0; x < WIDTH; ++x) {
      const offset = HEADER.length + y * STRIDE + (x >> 3), bit = 0x80 >> (x & 7);
      const black = bytes[offset] & bit, red = bytes[offset + PLANE] & bit;
      if (black && red) throw new Error('红黑位图重叠。');
      const pixel = (y * WIDTH + x) * 4;
      rgba[pixel] = red ? 174 : black ? 38 : 255;
      rgba[pixel + 1] = red ? 48 : black ? 40 : 255;
      rgba[pixel + 2] = red ? 40 : black ? 35 : 255;
      rgba[pixel + 3] = 255;
    }
  }
  return rgba;
}
