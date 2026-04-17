import { getGridState } from '$lib/server/grid';

function hexToRgb(hex: string): string {
	const r = parseInt(hex.slice(1, 3), 16);
	const g = parseInt(hex.slice(3, 5), 16);
	const b = parseInt(hex.slice(5, 7), 16);
	return (
		String(r).padStart(3, '0') + '.' + String(g).padStart(3, '0') + '.' + String(b).padStart(3, '0')
	);
}

export const GET = async () => {
	const { grid } = await getGridState();
	const body = grid.map(hexToRgb).join(',');
	return new Response(body, {
		headers: { 'Content-Type': 'text/plain' }
	});
};
