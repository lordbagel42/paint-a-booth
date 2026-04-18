import type { RequestHandler } from './$types';

export const GET: RequestHandler = async ({ request, platform, locals }) => {
	const id = platform!.env.BOOTH_ROOM.idFromName('default');
	const stub = platform!.env.BOOTH_ROOM.get(id);
	const doUrl = new URL(request.url);
	if (locals.user) {
		doUrl.searchParams.set('userName', locals.user.name);
		doUrl.searchParams.set('userId', locals.user.id);
	}
	return stub.fetch(new Request(doUrl, { headers: request.headers }));
};
