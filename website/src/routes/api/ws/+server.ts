import type { RequestHandler } from './$types';

export const GET: RequestHandler = async ({ request, platform }) => {
	const id = platform!.env.BOOTH_ROOM.idFromName('default');
	const stub = platform!.env.BOOTH_ROOM.get(id);
	return stub.fetch(request);
};
