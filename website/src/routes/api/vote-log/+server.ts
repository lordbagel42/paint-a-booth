import type { RequestHandler } from './$types';
import { getDb } from '$lib/server/db';
import { voteHistory } from '$lib/server/db/schema';

export const POST: RequestHandler = async ({ request, locals, platform }) => {
	if (!locals.user) return new Response('Unauthorized', { status: 401 });
	const { cellIndex, color, direction } = (await request.json()) as Record<string, unknown>;
	if (typeof cellIndex !== 'number' || typeof color !== 'string' || typeof direction !== 'number')
		return new Response('Bad Request', { status: 400 });
	const db = getDb(platform!.env.DB);
	await db.insert(voteHistory).values({
		userId: locals.user.id,
		cellIndex,
		color,
		direction
	});
	return new Response('ok');
};
