export const COLORS = [
	{ hex: '#FF0000', name: 'Red' },
	{ hex: '#FF8000', name: 'Orange' },
	{ hex: '#FFFF00', name: 'Yellow' },
	{ hex: '#00FF00', name: 'Green' },
	{ hex: '#0000FF', name: 'Blue' },
	{ hex: '#800080', name: 'Purple' },
	{ hex: '#FFFFFF', name: 'White' },
	{ hex: '#000000', name: 'Black' }
] as const;

export const COLOR_HEXES: string[] = COLORS.map((c) => c.hex);
