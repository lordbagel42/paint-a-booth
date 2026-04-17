import { fail } from '@sveltejs/kit';
import { sql } from 'drizzle-orm';
import { db } from '$lib/server/db';
import { pixelVote } from '$lib/server/db/schema';
import { COLOR_HEXES } from '$lib/colors';
import { getGridState } from '$lib/server/grid';

export const load = async () => {
	return await getGridState();
};

export const actions = {
	vote: async ({ request }) => {
		const formData = await request.formData();
		const cellIndex = Number(formData.get('cellIndex'));
		const color = String(formData.get('color'));
		const direction = Number(formData.get('direction'));

		if (isNaN(cellIndex) || cellIndex < 0 || cellIndex > 63) return fail(400);
		if (!COLOR_HEXES.includes(color)) return fail(400);
		if (direction !== 1 && direction !== -1) return fail(400);

		await db
			.insert(pixelVote)
			.values({ cellIndex, color, score: direction })
			.onConflictDoUpdate({
				target: [pixelVote.cellIndex, pixelVote.color],
				set: { score: sql`${pixelVote.score} + ${direction}` }
			});

		return { success: true };
	}
};
